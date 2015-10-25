#!/usr/bin/python -u

import sys # so that we can return a value at the end.
import random # for randum number generators
import time
import getopt
import os.path
from testutils import *

pname = "cconv3"

def main(argv):
    retval = 0
    print "MPI cconv3 unit test"
    retval = 0
    usage = "Usage:\n"\
            "./testfft2.py\n"\
            "\t-s\t\tSpecify a short run\n"\
            "\t-h\t\tShow usage"

    shortrun = False
    try:
        opts, args = getopt.getopt(argv,"sh")
    except getopt.GetoptError:
        print "Error in arguments"
        print usage
        sys.exit(2)
    for opt, arg in opts:
        if opt in ("-s"):
            shortrun = True
        if opt in ("-h"):
            print usage
            sys.exit(0)

    logfile = 'testcconv3.log' 
    print "Log in " + logfile + "\n"
    log = open(logfile, 'w')
    log.close()

    start=11
    stop=64

    if not os.path.isfile(pname):
        print "Error: executable", pname, "not present!"
        retval += 1
    else:
        Alist = [2,4,8,16]
        Xlist = [2,1,3,4,5,6,7,8,9,10,random.randint(start,stop)]
        Ylist = [2,1,3,4,5,6,7,8,9,10,random.randint(start,stop)]
        Zlist = [2,1,3,10,random.randint(start,stop)]
        Plist = [8,4,3,2,random.randint(9,12),1]

        if(shortrun):
            Alist = [2,4]
            Xlist = [2,random.randint(6,32)]
            Ylist = [2,random.randint(6,32)]
            Zlist = [2,random.randint(6,32)]
            Plist = [2,random.randint(4,8)]
            
        timeout = 60

        ntests = 0
        ntests = len(Xlist) * len(Ylist) * len(Zlist) * len(Plist)
        print "Running", ntests, "tests."
        tstart = time.time()

        failcases = ""
        ntest = 0
        nfails = 0
        for P in Plist:
            for X in Xlist:
                for Y in Ylist:
                    for Z in Zlist:
                        for A in Alist:
                            ntest += 1
                            args = []
                            args.append("-x" + str(X))
                            args.append("-y" + str(Y))
                            args.append("-z" + str(Z))
                            args.append("-A" + str(A))
                            args.append("-N1")
                            args.append("-s1")
                            args.append("-a1")
                            args.append("-T1")
                            args.append("-t")
                            args.append("-q")
                            rtest, cmd = runtest(pname, P, args, logfile, timeout)
                            if not rtest == 0:
                                nfails += 1
                                failcases += " ".join(cmd)
                                failcases += "\t(code " + str(rtest) + ")"
                                failcases += "\n"

        if nfails > 0:
            print "\nFailure cases:"
            print failcases
            retval += 1
        print "\n", nfails, "failures out of", ntests, "tests." 

        tend = time.time()
        print "\nElapsed time (s):", tend - tstart
        
    sys.exit(retval)

if __name__ == "__main__":
    main(sys.argv[1:])
    
