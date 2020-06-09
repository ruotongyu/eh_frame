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
	binary=$(echo "$file" | cut -d'/' -f 7)
	tmp=${binary/.strip/}
	ref="gtRef_${tmp}"
	ref="${ref}.pb"
	block="gtBlock_${tmp}"
	block="${block}.pb"
	gtref=${file/$binary/$ref}
	gtblock=${file/$binary/$block}
	opt=$(echo "$file" | cut -d'/' -f 6 | cut -d'_' -f 2)
	flag="x64"
	if [ $opt = "m32" ]; then
		flag="x32"
	fi
	echo "Result for $file"
	#./dyninstBB_extent $file $gtref $gtblock $flag
	fnum=$((fnum+1))
done
echo "File Number: $fnum"
