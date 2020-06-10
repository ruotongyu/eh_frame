while getopts ":i:f" opt; do
	case $opt in
		i) 
			dirt=$OPTARG;;
		f)
			FLAG=$OPTARG;;
	esac
done
fnum=0

for file in `find $dirt -name "*.strip" | grep -v "shuffle"`; do
	binary=$(echo "$file" | cut -d'/' -f 6)
	tmp=${binary/.strip/}
	pb="ehRes_${tmp}"
	pb="${pb}.pb"
	gtRes=${file/$binary/$pb}
	opt=$(echo "$file" | cut -d'/' -f 5 | cut -d'_' -f 2)
	flag="x64"
	if [ $opt = "m32" ]; then
		flag="x32"
	fi
	echo "Pb File: $gtRes"
	./dyninstBB_extent $file $flag $gtRes
	fnum=$((fnum+1))
done
echo "File Number: $fnum"
