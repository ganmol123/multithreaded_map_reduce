# multithreaded_map_reduce

**Design Assumptions:**

- All workers will have access to the database which can be through NFS or a shared
    storage system.
- If there is a case of the master failing, assume that the client will invoke master again.
- If a worker fails, the master will recover it.

**Design:**

- On a high level, clients will only interact with the master node and also control the
    number of workers to invoke and threads per worker to run.
- Master will detect and recover the faulty worker using the fault tolerance mechanism
    implemented.
- The client Program will instantiate the Master Node module implemented in
    MapReduceMaster class. (Can be found under _master/include/MapReduceMaster.h)_
- The client will also implement the map and reduce functions in a class inherited from
    MapReduceInterface (Can be found under _master/include/MapReduceMaster.h_ )

The higher-level design diagram is as follows:


**API Interface Implementation:**

**-** MapReduceCpp is a header-only library containing the implementation of
    **-** MapReduceInterface and registration functionality for clients to register their own
       implementation of MapReduceInterface.
    **-** MapReduceMaster: Users mostly interact with this object for the map-reduce
       tasks.
    **-** Map worker and Reduce worker implementation: Each map worker that runs on
       one server is implemented by map_controller_module and the ‘reduce’ worker is
       implemented by reduce_controller_module.
- Let’s see how the Map-Reduce library is used in the InvertedIndex program(Can be
    found under _master/src/IntertedIndex.CPP_ ) **(*BonusTask: Ability to specify arbitrary**
    **map and reduce functions, as a "true" library implemented)**

```
//including the header library
include "MapReduceMaster.h"
```
```
class InvertedIndexMapReduce: public MapReduceInterface {
public:
void map_fn(string key, string value) {
```
```
// implements how each line of the data is processed.
// calls an inherited method emitIntermediate(key, value). This will push
the key value pair in an array
}
```
```
void reduce_fn(string key, vector<string> values) {
```
```
// implements how each key will be reduced
// call another inherited method emit(key, value). This will push the
reduced key value pair to another array
}
};
```
- Client registers the interface as follows:

```
MapReduceInterfaceFactoryRegistration<InvertedIndexMapReduce>
_InvertedIndexMapReduce("MapReduce");
```

- Client then created a MapReduceMaster object and invoked the process() function. We
    can see below that the constructor has 3 args
       - InputFileName: Text file with the test case.
          (master/testcases/WorkCounterInput.txt)
       - DataDirectory: The build folder is built after running the script. It has the output
          and the temporary files. The number of these may differ based on the number of
          workers selected.
       - Nr_worker: Number of workers per map and reduce.

```
int main() {
// set the parameters by configuration
int nr_workers;
string inputFileName;
string dataDirectory;
```
```
MapReduceMaster masterInstance(inputFileName, dataDirectory, nr_workers);
int result = masterInstance.process();
return 0;
}
```
#### RPC:

- Using a CPP based RPC library calledrpclib
- Implemented 2 new functions for each server I spawn in a different process. They are
    explained below and their implementation can be found at
    _master/include/MapReduceMaster.h_ :
       - Map_controller_module: Scans and reads each line if the line hashed to the
          mapper scanning the file and applies the map function. Creates a temp file,
          pushes the key-value pairs from the array and loops over emitted key-value pairs
          and hashes them to a reducer based on the key and pushes them to a temporary
          file. (explained further in implementation details)
       - Reduce_Controller_module: Searches the temp file with the associated ID. Loops
          over the text and maps each key with the associated [] of values. Now it loops
          over the key-value pairs and applies the reduce_fn which gives us the array
          which is further pushed to an output file. (explained further in implementation
          details)

**Requirements:**

### * Please make sure that this program is tested on aMac as written on the same & all dependencies mac specific.

- CMake 3.22 or >: I use CMake to build the library and the programs.
    Installation Instructions (Only Mac Specific):
       - Check if already installed: cmake - -version
       - If returns the version number > 3.22, you’re good to go!


- Else follow the instructions fromhere.
- If brew is already installed, run: brew install cmake
- CPP 3.15 or >

**File Structure & detail:**

- Config: Configuration files to set NWorker, InputFile and DataDirectory where the output
    will be stored.
- Include: Header files
- Lib: rpclib and google test
- Src: source file for WordCount and InvertedIndex
- Test: This has unit tests which used google test
- Testcase: Had the 2 input files to add test case (text).
- **Build** : This is generated on building the project (runscript.sh). Also has the output files.
- script.sh: This builds the library and the programs for WordCounter and InvertedIndex.
    Everything can be found inside the build folder.

**Instructions to run:**

- A single command can be used to build and run everything: script.sh
- Once the project is built, the WordCounter and InvertedIndex program can also be run
    independently by _./WordCounter and ./InvertedIndex_
- By default, I have added the text from Project Gutenberg as test cases. On running, the
    output will be generated for this text.

**Instructions to add/change test cases:**

- Simple go to _testcase/WordCounterText or testcase/InvertedIndexText_ and add the
    desired text to be tested.

**Output Generated Explanation:**

- The outputs are generated inside the output directories( _build/InvertedIndexData&_
    _build/WordCounterData_ ) of each of the programs insidethe build folder.
- The output directory will contain the input given and a bunch of temp and output files.
- The number of temp and output files depends on the number of workers set.
- The temporary files generated have the format - temp_[mapper]_[reducer].txt
- The output files generated have the format - output_[reducer].txt

**Example Applications/Testcases:**

- Parallelism Archived:


**_Running WorkCounter with NWorker = 2_**

**_Running WorkCounter with NWorker = 15_**

- For Text: Hello My name is Anmol Gupta and with N_Worker = 2
WordCounter Example:

InvertedIndex Example:


**Bonus: Fault Tolerance Implementation:**

- This will only work for at most one mapper or reducer failure.
- Implemented _is_map_done_ and _is_reduce_done_ functionsto continuously check for
    server failure.
- Ran the tests for this as follows:
    - Started WordCounter with N_Workers = 2
    - Ran _lsof -i -P | grep LISTEN | grep :$PORT_ to keeptrack of PID’s
    - Tried Killing a map and a reduce process
    - The experiment is shown below:

Here we can see that on Killin the PID 62561, the PORT is restarted. I tried doing this for 3
process IDs and we can see, that for all, the PORT is restarted and a new PID is assigned to
the process.

**Mandatory Requirements Achieved:**

- Implemented a distributed map-reduce library.
- Provided a comprehensive report
- Implemented WordCounter and Inverted Index
- Provided with a script to run everything just by one line.
- Provided config files
- Fault Tolerance (explained below)


**Assignment Bonus Requirements Achieved:**

- Fault Tolerance
- Ability to specify arbitrary map and reduce functions, as a "true" library implemented
    (check under API Interface Implementation)

**Constraints:**

- Single shared file used for processing

**References:**

- Google’s Map Reduce Paper
- Project Gutenberg

## NOTE: Kindly run the script and the build directory will be created

## with the output directory of both the programs and their temporary

## and final outputs.


