modes=("gcm" "cbc" "ctr" "ccm" "xts" "ofb" "cfb" "ecb")

for mode in "${modes[@]}"; do
    extra_flags=""
    if [ "$mode" == "ecb" ]; then
        extra_flags="--allow-ecb"
    fi

    # 1KB (1000 runs)
    .\build_windows_gcc\aestool.exe bench --mode "$mode" $extra_flags --key output/windows/key_bin/key.bin --size 1024 --runs 1000 --out output/windows/benchmarks/bench_windows_gcc_${mode}_1KB.csv
    
    # 4KB (1000 runs)
    .\build_windows_gcc\aestool.exe bench --mode "$mode" $extra_flags --key output/windows/key_bin/key.bin --size 4096 --runs 1000 --out output/windows/benchmarks/bench_windows_gcc_${mode}_4KB.csv
    
    # 16KB (1000 runs)
    .\build_windows_gcc\aestool.exe bench --mode "$mode" $extra_flags --key output/windows/key_bin/key.bin --size 16384 --runs 1000 --out output/windows/benchmarks/bench_windows_gcc_${mode}_16KB.csv
    
    # 256KB (100寧 runs -> 100 runs)
    .\build_windows_gcc\aestool.exe bench --mode "$mode" $extra_flags --key output/windows/key_bin/key.bin --size 262144 --runs 100 --out output/windows/benchmarks/bench_windows_gcc_${mode}_256KB.csv
    
    # 1MB (50 runs)
    .\build_windows_gcc\aestool.exe bench --mode "$mode" $extra_flags --key output/windows/key_bin/key.bin --size 1048576 --runs 50 --out output/windows/benchmarks/bench_windows_gcc_${mode}_1MB.csv
    
    # 8MB (20 runs)
    .\build_windows_gcc\aestool.exe bench --mode "$mode" $extra_flags --key output/windows/key_bin/key.bin --size 8388608 --runs 20 --out output/windows/benchmarks/bench_windows_gcc_${mode}_8MB.csv
done
