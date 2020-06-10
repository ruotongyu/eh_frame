while getopts ":i:f" opt; do
	case $opt in
		i) 
			dirt=$OPTARG;;
		f)
			FLAG=$OPTARG;;
	esac
done

for file in `find $dirt -name "*.strip" | grep -v "shuffle"`; do
	binary=$(echo "$file" | cut -d'/' -f 6)
	tmp=${binary/.strip/}
	pb="ehRes_${tmp}"
	pb="${pb}.pb"
	gt_block="gtBlock_${tmp}"
	gt_block="${gt_block}.pb"
	pbRes=${file/$binary/$pb}
	gtRes=${file/$binary/$gt_block}
	b_file=${file/$binary/$tmp}
	echo "python3 /home/binpang/binary_reasemble/disassemble_compare/extract_proto/compareFuncsEhRes.py -g $gtRes -i $pbRes -b $b_file"
	python3 /home/binpang/binary_reasemble/disassemble_compare/extract_proto/compareFuncsEhRes.py -g $gtRes -i $pbRes -b $b_file
done
