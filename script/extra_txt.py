


if __name__ == "__main__":
    f = open('/tmp/utils_bashRes', 'r')
    fp_num = 0
    for line in f:
        string = str(line).split(" ")
        if len(string) > 3:
            if string[1] == "False":
                fp_num += int(string[5].strip())
    print("Total number of fp functions:", fp_num)




