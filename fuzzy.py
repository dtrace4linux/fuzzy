#! /usr/bin/python3

"""
Example application
"""

import os
import glob
import sys
import argparse
import re

noext = { "o": 1, "cm": 1, "so": 1, "gif": 1, "png": 1, "bmp": 1}

def copy_file(src, dst):
    shutil.copyfileobj(src, dst)

def usage():
    parser.print_help()

def usage2():
    return"""
Description:

   This tool does xxx.

Examples:

  $ fuzzy.py -dir mydir/ -files hello
  $ fuzzy.py terminal
  $ fuzzy.py -s terminal term

"""

def display(res, pat):
#    print("score:", res["score"], res["file"], res["mask"])
    
    file = res["file"]
    mask = res["mask"]

    print("\033[37m%s - " % (res["score"]), end = "")

    b = os.path.basename(file)
    if len(b) != len(mask):
        print("error mask mismatch");
        print("  file='%s'" % file);
        print("  mask='%s'" % mask);
        return

    print("%s/" % os.path.dirname(file), end = "")
    for i in range(0, len(b)):
        ch = b[i]
        if mask[i] == 'X':
            print("\033[36m%s\033[37m" % ch, end = "")
        else:
            print(ch, end = "")
    print()

def fmatch(ret, b, pat, bol = 0, ic = 0):
#    print("fmatch=", b, pat, bol, ic)

    blst = b.split(".")
    plst = pat.split(".")
    ret[0] = ""

    j = 0
    m = ''
    for i in range(0, len(blst)):
        b1 = blst[i]
        if i >= len(plst):
            for i1 in range(i, len(blst)):
                m += "."
                m += "." * len(blst[i])
            ret[0] = m
            return m

        p1 = plst[i]
        mat = fmatch2(b1, p1, bol, ic)
        if mat == "" or mat is None:
#            print("...no match mat=", mat, b1, p1)
            return ""
        if m:
            m += "."
        #print("mat", mat, m)
        m += mat

    ret[0] = m
    return m

def fmatch2(b, pat, bol, ic):

#    print("  fmatch2: b=%s pat=%s bol=%d ic=%d" % (b, pat, bol, ic))

    str = ''
    i = 0
    j = 0
    while i < len(b) and j < len(pat):
        b1 = b[i]
        p1 = pat[j]

        if ic:
            b1 = b1.lower()
            p1 = p1.lower()

        if b1 == p1:
            str += "X"
            i += 1
            j += 1
            continue

        if p1 == "*":
            i = i + 1
            found = 0
            while i < len(b):
                b1 = b[i]
                if ic:
                    b1 = b1.lower();
                if b1 == p1:
                    i = i + 1
                    found = 1
                    break
                i = i + 1
                str += " "
            if not found:
                break
            j += 1
            continue

        i += 1
        str += "_"

    if j < len(pat):
#        print("..fmatch2 - no match j=", j, "b=", b)
        return ""

    while i < len(b):
        str += "_"
        i += 1

    if bol and str[0] != 'X':
        return ""
    return str



def main():
    try:
        main2()
    except:
        print("Stack Trace:")
        exc_type, exc_value, tb = sys.exc_info()
        if tb is not None:
            prev = tb
            curr = tb.tb_next
            lv = 0
            import traceback
            traceback.print_exception(exc_type, exc_value, tb,
                              limit=0, file=sys.stdout)
            s = []
            while curr is not None:
                s.append(curr)
                curr = curr.tb_next
            s.reverse()
            for frame in s:
                print("[%2d] %s:%d %s()" % (lv, 
                        frame.tb_frame.f_code.co_filename, 
                        frame.tb_frame.f_lineno,
                        frame.tb_frame.f_code.co_name, 
                        ))
                f = open(frame.tb_frame.f_code.co_filename, "r")
                for i in range(0, frame.tb_frame.f_lineno-1):
                    f.readline()
                print("%3d: %s" % (frame.tb_frame.f_lineno, f.readline()), )
                for v in sorted(frame.tb_frame.f_locals):
                    print("  %10s = %s" % (v, frame.tb_frame.f_locals[v]))
                lv += 1

def main2():
    """
    Main entry point.
    """

    global parser

    parser = argparse.ArgumentParser(
        description="fuzzy - find files matching fuzzy expression",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=usage2()
    )
    parser.add_argument(
        "-debug", action="store_true", help="Turn on debugging")
    parser.add_argument(
        "-dir", nargs=1, help="Scan specified directory for list of filenames")
    parser.add_argument(
        "-files", action="store_true", help="Include files in -dir scan")
    parser.add_argument(
        "-f", nargs=1, help="Specify a file containing list of files to match against")
    parser.add_argument(
        "-n", action="store_true", help="dry run")
    parser.add_argument(
        "-s", nargs=1, help="Add specified name to the list to scan. Useful for debugging")
    parser.add_argument(
        "words", nargs="*")

    args = parser.parse_args()
    if args.n:
        print("dry-run")

    if len(sys.argv) < 2:
        usage()
        sys.exit(1)

    files = {}

    need_path = 1
    if args.dir:
        need_path = 0
        type = ""
        if args.files:
            type = " -type f"
        for w in args.dir:
            for w1 in w.split(","):
                cmd = "find " + type + w1
                #print("cmd=", cmd)
                f = os.popen("find " + w1 + type )
                for ln in f:
                    files[ln.strip()] = 1
                    print("ln=" + ln.strip());

    if args.f:
        need_path = 0
        for fn in args.f:
            fh = open(fn, "r")
            for ln in fh:
                files[ln.strip()] = 1

    if args.s:
        need_path = 0
        for s in args.s:
            files[s] = 1

    if need_path:
        for wd in os.environ["PATH"].split(":"):
            for f in glob.glob(wd + "/*"):
                e = os.path.basename(wd)
                #print(f)
                if e not in noext:
                    files[f] = 1

    # Remove unwanted file types
    regexp = re.compile('\.(.*)$')
    for f in files:
        e = os.path.basename(f)
        m = regexp.match(e)
        if m is not None and m.group() in noext:
            files.pop(m.group())

    pat = args.words[0]
    dpat = re.sub("(.)", '\\1*', pat)
    #print("dpat=", dpat)

    results = {}
    for f in sorted(files):
        b = os.path.basename(f)
        ret = [""]

        if b == pat:
            results[f] = { "file": f, "score": 10, "mask": "X" * len(b)}
        
        elif b.lower() == pat.lower():
            results[f] = { "file": f, "score": 9, "mask": "X" * len(b)}

        elif fmatch(ret, b, pat, bol = 1):
            results[f] = { "file": f, "score": 8, "mask": ret[0]}

        elif fmatch(ret, b, pat, bol = 1, ic = 1):
            results[f] = { "file": f, "score": 7, "mask": ret[0]}

        elif fmatch(ret, b, pat):
            results[f] = { "file": f, "score": 6, "mask": ret[0]}

        elif fmatch(ret, b, pat, ic = 1):
            results[f] = { "file": f, "score": 5, "mask": ret[0]}

        elif fmatch(ret, b, dpat, ic = 1):
            results[f] = { "file": f, "score": 4, "mask": ret[0]}

    # Now print the results
    for r in sorted(results, key = results.score):
        display(results[r], pat)

# - - - - - - - - - - - - #
if __name__ == '__main__':
    main()

