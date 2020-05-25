input="/tmp/eh_frame_negative.log"
while IFS= read -r line
do
	SUB=$(echo "$line" | cut -d':' -f 5)
	NUM=$(echo "$SUB" | cut -d'#' -f 2)
	path=$(echo "$line" | cut -d':' -f 1)
	tmp=$(echo "$path" | cut -d'@' -f 3) 
	res=${path//@//}
	res=${res/./}
	if [ $tmp = "utils" ]; then
		file=$(echo "$path" | cut -d'@' -f 6)
	else
		file=$(echo "$path" | cut -d'@' -f 5)
	fi
	gt="gtRef_${file}"
	gt="${gt}.pb"
	pb=${res/$file/$gt}
	res="${res}.strip"
	#echo $res
	#echo $pb
	if [ $NUM = "0]" ]; then
		echo "Result for $res"
		./dyninstBB_extent $res $pb
	fi
done < "$input"
