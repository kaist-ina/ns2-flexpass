#!/usr/bin/python3

import subprocess
import sys
import time
import threading
import os
import signal
import argparse
import hashlib
from argparse import ArgumentParser
import time
import re
import json

def getParser():
    parser = ArgumentParser()
    parser.add_argument("--basefile", default="scripts/large-scale.tcl")
    parser.add_argument("--dry-run", action="store_true")
    parser.add_argument("--print-fct", action="store_true")
    parser.add_argument("-n", "--numFlow", type=int, default=10000)
    parser.add_argument("--workload", choices=('cachefollower', 'mining', 'search', 'webserver', 'me', 'google', 'facebook'), default='cachefollower')
    parser.add_argument("--linkLoad", type=float, default=0.5)
    parser.add_argument("-I", "--expID", type=int, default=1000)
    parser.add_argument("--deployStep", type=int, choices=range(0, 101), default=100)
    parser.add_argument("-c", "--cccType", choices=('xpass', 'gdx', 'mqgdx', 'negdx'))
    parser.add_argument("--trafficLocality", type=int, choices=range(0, 101), default=0)
    parser.add_argument("--deployPerCluster", type=int, choices=(0, 1), default=1)
    parser.add_argument("--foregroundFlowRatio", type=float, default=0.0)
    parser.add_argument("--enableSharedBuffer", type=int, choices=(0, 1), default=1)
    parser.add_argument("--egdxScheme", type=int, choices=(1, 2, 3, 4), default=2)
    parser.add_argument("--egdxSelDropThresh", type=int, default=150000)
    parser.add_argument("--xpassQueueWeight", type=float, default=0.5)
    parser.add_argument("--newEgdxAllocationLogic", type=int, choices=(0, 1), default=1)
    parser.add_argument("--tcpInitWindow", type=int, default=5)
    parser.add_argument("--egdxReactiveInitWindow", type=int, default=5)
    parser.add_argument("--reorderingMeasureInRc3", type=int, choices=(0, 1), default=0)
    parser.add_argument("--staticAllocation", type=int, choices=(0, 1), default=0)
    parser.add_argument("--xpassQueueIsolation", type=int, choices=(0, 1), default=0)
    parser.add_argument("--strictHighPriority", type=int, choices=(0, 1), default=0)
    parser.add_argument("--foregroundFlowSize", type=int, default=8000)
    parser.add_argument("--numForegroundFlowPerHost", type=int, default=4)
    parser.add_argument("--oracleQueueWeight", type=int, choices=(0, 1), default=0)
    parser.add_argument("--trueOracleWeight", type=int, choices=(0, 1), default=0)

    return parser

tasks = {}
g_tid = 0

def popenAndCall(onExit, popenArgs):
    global g_tid, tasks
    g_tid += 1
    def runInThread(onExit, tid, popenArgs):
        proc = subprocess.Popen(popenArgs)
        tasks[tid][1] = proc.pid
        ret = proc.wait()
        onExit(tid, ret)
        return
    thread = threading.Thread(target=runInThread, args=(onExit, g_tid, popenArgs))
    return thread, g_tid

def finishTask(tid, return_code):
    global tasks
    tasks[tid][2] = return_code

def sigint_handler(sig, frame):
    for _, pid, _ in tasks.values():
        os.kill(pid, signal.SIGTERM)

def parseLine(line, options):
    if '###!!' not in line:
        return line

    target = line.split('###!!')[-1].lstrip()  # which should still hold \n here

    for key, val in vars(options).items():
        if val is not None:
            target = target.replace('<<<' + key + '>>>', str(val))

    if ('<<<' in target) or ('>>>' in target):
        target = line
        print("Error: Cannot replace the line %s. Value not specified." % (line.strip()), file=sys.stdout)
        sys.exit(1)
    return target

def main():
    parser = getParser()
    options, args = parser.parse_known_args()

    basefile_content = []
    try:
        with open(options.basefile, 'r') as f:
            basefile_content = f.readlines()
    except (OSError, FileNotFoundError):
        print("Basefile %s not found." % options.basefile, file=sys.stderr)
        sys.exit(1)

    for idx, line in enumerate(basefile_content):
        parsed = parseLine(line, options)
        assert (len(parsed) == 0 or parsed[-1] == '\n')
        basefile_content[idx] = parsed

    str_config = "".join(basefile_content)
    target_path = os.path.join("/tmp", hashlib.sha1(str_config.encode('utf-8')).hexdigest()+'_'+ str(int(time.time()*1000.0)%100000) + ".tmp")
    try:
        with open(target_path, 'w') as f:
            f.write(str_config)
    except (OSError, FileNotFoundError):
        print("Cannot write configuration to %s." % target_path, file=sys.stderr)
        sys.exit(1)
    print(target_path, file=sys.stderr)


    arg_dict = {}
    for a in vars(options):
        arg_dict[a] = getattr(options, a)
    if options.dry_run:
        print(str_config)
        print(json.dumps(arg_dict), file=sys.stderr)
        return

    signal.signal(signal.SIGINT, sigint_handler)
    os.system("chmod +x ./ns")
    try:
        arguments = ['./ns', target_path]
        arguments.extend(args)
        thr, tid = popenAndCall(finishTask, arguments)
        tasks[tid] = [thr, 0, 0]
        thr.start()
        thr.join()

        if options.print_fct:
            try:
                print("")
                print("==========================ARG")
                print(json.dumps(arg_dict))
                print("==========================FCT")
                with open('outputs/fct_%d.out' % options.expID, 'r') as fct_file:
                    print(fct_file.read(), end="")
            except:
                pass
        sys.exit(tasks[tid][2])
    except (OSError, KeyboardInterrupt):
        sigint_handler(signal.SIGINT, None)


if __name__ == "__main__":
    main()
