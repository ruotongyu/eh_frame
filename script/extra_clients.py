


if __name__ == "__main__":
    f = open('/home/binpang/Desktop/dyninst/Result/servers.fn', 'r')
    raw_num = 0
    func_num = 0
    eh_num = 0
    scan_num = 0
    WithRef = 0
    NoRef = 0
    linker = 0
    pcThunk = 0
    for line in f:
        string = str(line).split(" ")
        if len(string) > 6:
            if string[2] == "Functions":
                func_num += int(string[7].strip())
            if string[5] == "raw":
                raw_num += int(string[8].strip())
            if string[5] == "linker":
                linker += int(string[7].strip())
            if string[5] == "pcThunk":
                pcThunk += int(string[7].strip())
            if string[5] == "ehframe":
                eh_num += int(string[7].strip())
            if string[5] == "scan":
                scan_num += int(string[8].strip())
            if len(string) > 7:
                if string[7] == "With":
                    WithRef += int(string[9].strip())
                if string[7] == "WithOut":
                    NoRef += int(string[9].strip())

    print("Total number of functions:", func_num)
    print("Total number of functions identified in eh:", raw_num - eh_num)
    print("Total number of linker functions:", linker)
    print("Total number of pcthunk functions:", pcThunk)
    print("Total number of functions identified in scan:", eh_num - scan_num)
    print("Total number of FN functions:", scan_num)
    print("Total number of FN in Gaps With Ref:", WithRef)
    print("Total number of FN in Gaps Without Ref:", NoRef)




