// using this to replace preprocessor code
#pragma once
#include <iostream>
#include <unistd.h>
#include <vector>
#include <algorithm>
#include <iterator>
#include <string>
#include <thread>
#include <future>
#include <functional>
#include <map>
#include <unordered_map>
#include <utility>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include "rpc/server.h"
#include "rpc/client.h"
#include "rpc/this_server.h"
#include "rpc/this_session.h"
#include "Utility.h"

// declaration of Interface class that needs registration
class MapReduceInterface;
typedef MapReduceInterface *(*mapReduceInterfaceGenerator)();

class MapReduceInterfaceFactory
{
public:
    static MapReduceInterfaceFactory &get()
    {
        static MapReduceInterfaceFactory instance;
        return instance;
    }
    MapReduceInterface *getMapReduceInterface(const char *typeName)
    {
        auto it = m_generators.find(typeName);
        if (it != m_generators.end())
        {
            return it->second();
        }
        return nullptr;
    }
    bool registerGenerator(const char *typeName,
                           const mapReduceInterfaceGenerator &funcCreate)
    {
        return m_generators.insert(make_pair(typeName, funcCreate)).second;
    }

private:
    MapReduceInterfaceFactory() {}
    MapReduceInterfaceFactory(const MapReduceInterfaceFactory &);
    ~MapReduceInterfaceFactory() {}
    unordered_map<string, mapReduceInterfaceGenerator> m_generators;
};

// Class that does the registration through factory
template <typename T>
class MapReduceInterfaceFactoryRegistration
{
public:
    MapReduceInterfaceFactoryRegistration(const char *id)
    {
        MapReduceInterfaceFactory::get().registerGenerator(
            id,
            []()
            { return static_cast<MapReduceInterface *>(new T()); });
    }
};

// This interface is for clients to communicate how map and reduce is to be done
class MapReduceInterface
{
public:
    // CONTR
    MapReduceInterface()
    {
    }
    // Copy CONTR
    MapReduceInterface(const MapReduceInterface &orig)
    {
        copy(orig.emitted_intermediates.begin(),
             orig.emitted_intermediates.end(),
             back_inserter(emitted_intermediates));

        copy(orig.emitted_outputs.begin(),
             orig.emitted_outputs.end(),
             back_inserter(emitted_outputs));
    }
    // declaration is map_fn & reduce_fn, should be implemented by child class.
    virtual void map_fn(string k1, string v1) = 0;
    virtual void reduce_fn(string k2, vector<string> v2_list) = 0;

protected:
    // Emits the intermediate key-value pairs found by map_fn logic.
    void emitIntermediate(string k2, string v2)
    {
        emitted_intermediates.push_back(make_pair(k2, v2));
    }
    // Emits the key-list_of_values found by reduce_fn logic.
    void emit(string k2, vector<string> v3_list)
    {
        emitted_outputs.push_back(make_pair(k2, v3_list));
    }

public:
    vector<pair<string, string> > emitted_intermediates;
    vector<pair<string, vector<string> > > emitted_outputs;
};

// returns if the server to which the client is connected is not listening anymore
bool is_server_down(rpc::client *client)
{
    return (client->get_connection_state() ==
            rpc::client::connection_state::disconnected);
}

bool map_completed = false;
bool reduce_completed = false;

// Forward declaration of map_controller_module & reduce_controller_module
int map_controller_module(string inputFileName,
                          string dataDirectory,
                          int nr_mapper,
                          int nr_reducer,
                          int mapper_id);
int reduce_controller_module(string dataDirectory,
                             int nr_reducer,
                             int nr_mapper,
                             int reducer_id);

class MapReduceMaster
{
public:
    // CTOR: takes input file name and the directory where the file is located.
    MapReduceMaster(string f_name,
                    string out_dir) : inputFileName(f_name),
                                      outputResultDirectory(out_dir) {}

    // Overloaded constructor that N representing nr_mapper == nr_reducer
    MapReduceMaster(string f_name,
                    string out_dir,
                    int N) : inputFileName(f_name),
                             outputResultDirectory(out_dir),
                             nr_mapper(N),
                             nr_reducer(N) {}

    // Overloaded constructor that accepts nr_mapper and nr_reducer
    MapReduceMaster(string f_name,
                    string out_dir,
                    int nr_mapper,
                    int nr_reducer) : inputFileName(f_name),
                                      outputResultDirectory(out_dir),
                                      nr_mapper(nr_mapper),
                                      nr_reducer(nr_reducer) {}
    // The main function that the client calls to start the processing.
    int process()
    {
        // Create nr_mapper == nr_reducer rpc servers (each is one map-reduce worker running in a different process)
        int basePort = 8420; // This is the port on which first workers binds to. Rest of the workers will bind to port number which is 1 more than the previous worker.

        for (int i = 0; i < nr_mapper; i++)
        {
            // For each server, we create one process that runs independently on that server.
            pid_t pID = fork();
            if (pID == 0)
            {
                cout << "Starting Server: PID=" << getpid()
                     << ", PORT=" << basePort + i << endl;
                rpc::server srv(basePort + i);
                srv.bind("map", [&](int idx)
                         {
                    int e = map_controller_module(this->inputFileName,
                                                  this->outputResultDirectory,
                                                  this->nr_mapper,
                                                  this->nr_reducer,
                                                  idx);
                    map_completed = true;
                    return; });
                srv.bind("reduce", [&](int idx)
                         {
                    int e = reduce_controller_module(this->outputResultDirectory,
                                                     this->nr_reducer,
                                                     this->nr_mapper,
                                                     idx);
                    reduce_completed = true;
                    return; });
                srv.bind("exit", [&]()
                         { rpc::this_session().post_exit(); });
                srv.bind("is_map_done", [&]()
                         { return map_completed; });
                srv.bind("is_reduce_done", [&]()
                         { return reduce_completed; });
                srv.bind("stop_server", [&]()
                         {
                    rpc::this_server().stop();
                    exit(0); });
                srv.async_run(2);
                int b;
                cin >> b; // blocking call.
                return 0;
            }
        }

        // creating client pool, bind to the port where the servers are listening. Wait for 1 second so that all servers are completely started
        sleep(1);
        vector<rpc::client *> client_pool;
        cout << "Client binding complete!" << endl;
        for (int i = 0; i < nr_mapper; i++)
        {
            client_pool.push_back(new rpc::client("localhost", basePort + i));
        }
        cout << "Client binding....." << endl;
        vector<future<clmdep_msgpack::v1::object_handle> > map_future_pool;
        for (int worker_idx = 0; worker_idx < nr_mapper; worker_idx++)
        {
            map_future_pool.push_back(client_pool[worker_idx]->async_call(
                "map", worker_idx));
        }
        cout << "Mapper invoked!" << endl;

        // Implementation of heart-beat mechanism
        vector<int> map_task_status = vector<int>(nr_mapper, 0);
        int nr_map_done = 0;
        while (true)
        {
            sleep(1);
            for (int worker_idx = 0; worker_idx < nr_mapper; worker_idx++)
            {
                if (map_task_status[worker_idx] == 1)
                {
                    continue;
                }
                if (is_server_down(client_pool[worker_idx]))
                {
                    cout << "Server failure at PORT="
                         << basePort + worker_idx << endl;
                    pid_t pID = fork();
                    if (pID == 0)
                    {
                        cout << "Server starting on PID=" << getpid() << endl;
                        rpc::server srv(basePort + worker_idx);
                        srv.bind("map", [&](int idx)
                                 {
                            int e = map_controller_module(
                                        this->inputFileName,
                                        this->outputResultDirectory,
                                        this->nr_mapper,
                                        this->nr_reducer,
                                        idx);
                            map_completed = true;
                            return; });
                        srv.bind("reduce", [&](int idx)
                                 {
                            int e = reduce_controller_module(
                                        this->outputResultDirectory,
                                        this->nr_reducer,
                                        this->nr_mapper,
                                        idx);
                            reduce_completed = true;
                            return; });
                        srv.bind("exit", [&]()
                                 { rpc::this_session().post_exit(); });
                        srv.bind("is_map_done", [&]()
                                 { return map_completed; });
                        srv.bind("is_reduce_done", [&]()
                                 { return reduce_completed; });
                        srv.bind("stop_server", [&]()
                                 {
                            rpc::this_server().stop();
                            exit(0); });
                        srv.async_run(2);
                        int b;
                        cin >> b; // blocking call.
                        return 0;
                    }
                    cout << "Server at PORT = " << basePort + worker_idx
                         << " restarted" << endl;
                    sleep(1);
                    client_pool[worker_idx] = new rpc::client("localhost",
                                                              basePort + worker_idx);
                    // Invoke the map task on this server.
                    map_future_pool[worker_idx] =
                        client_pool[worker_idx]->async_call("map", worker_idx);
                    cout << "Map task at PORT = " << basePort + worker_idx
                         << " restarted" << endl;
                    continue;
                }
                // At this point, the server is still alive and processing. Check if this server is done with the map task.
                bool map_status =
                    client_pool[worker_idx]->call("is_map_done").as<bool>();
                if (map_status)
                {
                    map_task_status[worker_idx] = 1;
                    nr_map_done += 1;
                }
                else
                {
                    cout << worker_idx << " not yet completed" << endl;
                }
            }
            // Test if all map tasks are successfully done
            if (nr_map_done == nr_mapper)
            {
                break;
            }
        }
        cout << "Mapping success by all servers!!" << endl;

        // Now ask server for reduce task
        vector<future<clmdep_msgpack::v1::object_handle> > reduce_future_pool;
        for (int worker_idx = 0; worker_idx < nr_mapper; worker_idx++)
        {
            reduce_future_pool.push_back(client_pool[worker_idx]->async_call(
                "reduce", worker_idx));
        }
        cout << "Invoking reducer tasks now!" << endl;

        // almost same as map task above
        vector<int> reduce_task_status = vector<int>(nr_reducer, 0);
        int nr_reduce_done = 0;
        while (true)
        {
            sleep(1);
            for (int worker_idx = 0; worker_idx < nr_mapper; worker_idx++)
            {
                if (reduce_task_status[worker_idx] == 1)
                {
                    continue;
                }
                if (is_server_down(client_pool[worker_idx]))
                {
                    cout << "Server failure at PORT = "
                         << basePort + worker_idx << endl;
                    pid_t pID = fork();
                    if (pID == 0)
                    {
                        cout << "Server starting on PID = " << getpid() << endl;
                        rpc::server srv(basePort + worker_idx);
                        srv.bind("map", [&](int idx)
                                 {
                            int e = map_controller_module(
                                        this->inputFileName,
                                        this->outputResultDirectory,
                                        this->nr_mapper,
                                        this->nr_reducer,
                                        idx);
                            map_completed = true;
                            return; });
                        srv.bind("reduce", [&](int idx)
                                 {
                            int e = reduce_controller_module(
                                        this->outputResultDirectory,
                                        this->nr_reducer,
                                        this->nr_mapper,
                                        idx);
                            reduce_completed = true;
                            return; });
                        // [TODO]: Probably remove, we dont need this.
                        srv.bind("exit", [&]()
                                 { rpc::this_session().post_exit(); });
                        srv.bind("is_map_done", [&]()
                                 { return map_completed; });
                        srv.bind("is_reduce_done", [&]()
                                 { return reduce_completed; });
                        srv.bind("stop_server", [&]()
                                 {
                            rpc::this_server().stop();
                            exit(0); });
                        // Run this server in two thread. One thread will handle
                        // the map reduce, the other will be used to signal the
                        // master the status of global work_done variable. This
                        // is a part of heart-beat mechanism.
                        srv.async_run(2);
                        int b;
                        cin >> b; // This is a blocking call.
                        return 0;
                    }
                    cout << "Server at PORT = " << basePort + worker_idx
                         << " restarted" << endl;
                    sleep(1);
                    client_pool[worker_idx] = new rpc::client("localhost",
                                                              basePort + worker_idx);
                    // Invoke the reduce task on this server.
                    reduce_future_pool[worker_idx] =
                        client_pool[worker_idx]->async_call("reduce", worker_idx);
                    cout << "Reduce task at PORT = " << basePort + worker_idx
                         << " restarted" << endl;
                    continue;
                }
                // At this point, the server is still alive and processing.
                // Check if this server is done with the reduce task.
                bool reduce_status =
                    client_pool[worker_idx]->call("is_reduce_done").as<bool>();
                if (reduce_status)
                {
                    reduce_task_status[worker_idx] = 1;
                    nr_reduce_done += 1;
                }
                else
                {
                    cout << worker_idx << " not yet completed" << endl;
                }
            }
            // Test if all reduce tasks are successfully done
            if (nr_reduce_done == nr_reducer)
            {
                break;
            }
        }

        // Now stop all the server
        for (int worker_idx = 0; worker_idx < nr_mapper; worker_idx++)
        {
            cout << "Stopping Server: PORT = " << basePort + worker_idx << endl;
            auto f = client_pool[worker_idx]->async_call("stop_server");
        }
        cout << "Reducer success by all servers!!" << endl;
        // stop all servers
        sleep(1);
        return 0;
    }

public:
    std::string inputFileName;
    std::string outputResultDirectory;
    int nr_mapper{1};
    int nr_reducer{1};
};

// Hashes a data type (int, float, double, string) in the range of 0 to max_range-1
template <typename d>
int hash_in_range(d data, int max_range)
{
    hash<d> hasher;
    return hasher(data) % max_range;
}

int map_controller_module(string inputFileName,
                          string dataDirectory,
                          int nr_mapper,
                          int nr_reducer,
                          int mapper_id)
{
    MapReduceInterface *map_reduce_fn =
        MapReduceInterfaceFactory::get().getMapReduceInterface("MapReduce");
    int record_number = 0;
    // adding time to check fault tolerance
    // sleep(10);
    ifstream file(dataDirectory + "/" + inputFileName);
    if (!file.good())
    {
        cout << "Could not find file " << inputFileName << " in directory "
             << dataDirectory
             << ". Double check if file or directory is correct" << endl;
        return -1;
    }
    string str;

    while (getline(file, str))
    {
        if (hash_in_range(record_number, nr_mapper) == mapper_id)
        {
            try
            {
                map_reduce_fn->map_fn(to_string(record_number), str);
            }
            catch (int e)
            {
                cout << "Exception occured while applying map function" << endl;
                return -2;
            }
        }
        record_number += 1;
    }

    vector<ofstream> temp_files;
    for (int reducer_id = 0; reducer_id < nr_reducer; reducer_id++)
    {
        temp_files.push_back(ofstream(dataDirectory + "/temp_" + to_string(mapper_id) + "_" + to_string(reducer_id) + ".txt"));
    }

    int reducer_idx;
    for (auto elem : map_reduce_fn->emitted_intermediates)
    {
        reducer_idx = hash_in_range(elem.first, nr_reducer);
        temp_files[reducer_idx] << elem.first << " " << elem.second << '\n';
    }
    // close all files
    for (int reducer_id = 0; reducer_id < nr_reducer; reducer_id++)
    {
        temp_files[reducer_id].close();
    }
    return 0;
}

int reduce_controller_module(string dataDirectory,
                             int nr_reducer,
                             int nr_mapper,
                             int reducer_id)
{
    MapReduceInterface *map_reduce_fn =
        MapReduceInterfaceFactory::get().getMapReduceInterface("MapReduce");
    string tempFile;

    // sleep(5);
    map<string, vector<string> > key_value_temp;
    map<string, vector<string> > key_value_acc;
    for (int mapper_idx = 0; mapper_idx < nr_mapper; mapper_idx++)
    {
        tempFile = dataDirectory + "/temp_" + to_string(mapper_idx) + "_" + to_string(reducer_id) + ".txt";
        try
        {
            key_value_temp = read_text<string, string>(tempFile);
        }
        catch (int e)
        {
            cout << "Could not find directory " << dataDirectory
                 << ". Double check if directory is correct" << endl;
            return -1;
        }
        // Accumulated this data in key_value_accumulated by extending the value vector of key_value_accumulated
        for (auto elem : key_value_temp)
        {
            // If this key already existed in the key_value_accumulated, then extend the values
            if (key_value_temp.count(elem.first) != 0)
            {
                key_value_acc[elem.first].insert(
                    key_value_acc[elem.first].end(),
                    elem.second.begin(),
                    elem.second.end());
            }
            else
            // Create an entry in key_value_accumulated
            {
                key_value_acc.insert(pair<string, vector<string> >(
                    elem.first, elem.second));
            }
        }
    }
    // Now apply reduction, the key-list_of_values will be emitted in the vector
    for (auto elem : key_value_acc)
    {
        try
        {
            map_reduce_fn->reduce_fn(elem.first, elem.second);
        }
        catch (int e)
        {
            cout << "Exception occured while applying reduce function" << endl;
            return -3;
        }
    }
    // Write the data to output text file in the dataDirectory
    write_key_val_vector(dataDirectory + "/output_" + to_string(reducer_id) + ".txt", map_reduce_fn->emitted_outputs);
    // return the success signal from this worker
    return 0;
}
