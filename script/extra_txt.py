


if __name__ == "__main__":
    f = open('/tmp/bashtest', 'r')
    num_gap = 0
    func_in_gap = 0
    num_dis_func = 0
    tp_func = 0
    fn_func = 0
    for line in f:
        if "number of gaps" in str(line):
           num = int(str(line).split(":")[1])
           num_gap += num
        if "functions in gaps" in str(line):
           num = int(str(line).split(":")[1])
           func_in_gap += num
        if "disassemble" in str(line):
           num = int(str(line).split(":")[1])
           num_dis_func += num
        if "correct functions" in str(line):
           num = int(str(line).split(":")[1])
           tp_func += num

        if "False Positive" in str(line):
           num = int(str(line).split(":")[1])
           fn_func += num
    
    print("Total numer of gaps:", num_gap)
    print("Total number of functions in gaps:", func_in_gap)
    print("Total number of functions from disassemble", num_dis_func)
    print("Total number of correct functions:", tp_func)
    print("Total number of functions in gaps:", fn_func)




