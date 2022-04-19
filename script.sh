# Build the library and the programs in build directory
echo "------------------------------------------------------------------------------"
echo "Building MapReduceCpp library and build directory............................."
echo "------------------------------------------------------------------------------"
mkdir build
cd build
cmake ..
cmake --build .

# Create the directory for each program and copy the input data into those directory.
echo "-----------------------------------------------------------------"
echo "Copying WordCounter input to WordCounterData directory..........."
echo "-----------------------------------------------------------------"
mkdir WordCounterData
cp ../config/config_WordCounter.txt .
cp ../testcase/WordCounterInput.txt WordCounterData
echo "-------------------------------------------------------------------------------------------------------"
echo "Executing WordCounter program, the temporary and the outputs are generated in WordCounterData directory"
echo "-------------------------------------------------------------------------------------------------------"
./WordCounter

echo "---------------------------------------------------------------------"
echo "Copying InvertedIndex input to InvertedIndexData directory..........."
echo "---------------------------------------------------------------------"
mkdir InvertedIndexData
cp ../config/config_InvertedIndex.txt .
cp ../testcase/InvertedIndexInput.txt InvertedIndexData
echo "-----------------------------------------------------------------------------------------------------------"
echo "Executing InvertedIndex program, the temporary and the outputs are generated in InvertedIndexData directory"
echo "-----------------------------------------------------------------------------------------------------------"
./InvertedIndex

echo "------------------"
echo "All tasks are done"
echo "------------------"

