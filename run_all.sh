#libs to be updated
declare -a files=( "libs/")
declare -a options=("gcc_O0 gcc_O2 gcc_O3 gcc_Os gcc_Of gcc_m32_O0 gcc_m32_O2 gcc_m32_O3 gcc_m32_Os gcc_m32_Of ccr_O0 ccr_O2 ccr_O3 ccr_Os ccr_Of")

for val in ${files[@]}; do
	cmd="run_clients_ehframe.sh -i /data/testsuite/"
       	cmd="${cmd}$val"	
	for opt in ${options[@]}; do
		res="${cmd}$opt"
		echo "Run Command: $res"
		bash $res
	done
done
#bash run_utils_ehframe.sh -i /data/testsuite/utils/binutils/gcc_O0 
