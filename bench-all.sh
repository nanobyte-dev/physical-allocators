MEM_SIZES=(32 128 512 1024)
BLOCK_SIZE1_RATIOS=(0 25 50 75 100)

HEADER="Test;Memory size;Block 1 ratio;Average (us);Low 95th (us);High 95th (us);Std dev (us)"

CONFIG="src/benchmark/Config.hpp"
RESULTS="bench-all.csv"

srand=123456

echo "$HEADER" > $RESULTS

for mem_size in "${MEM_SIZES[@]}"
do
    for block_size1_ratio in "${BLOCK_SIZE1_RATIOS[@]}"
    do
        echo ">>> Testing mem_size=${mem_size} block_size1_ratio=${block_size1_ratio}"

        export srand
        export mem_size
        export block_size1_ratio
        envsubst < "${CONFIG}.in" > "${CONFIG}"
        make clean
        make -j 12

        ./build/benchmark/benchmark | tee -a $RESULTS
    done
done
