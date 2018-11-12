#!/usr/bin/env python3.5m
# -*- coding: utf-8 -*-

import sys, os, time
import errno
import stat
import argparse

from ocd_rpc import OpenOcd

from addrIntercept import proccess, initOCD


parser = argparse.ArgumentParser(description='OCD client')

parser.add_argument("-in_", help="in fifo  addr interepter", default='out.fifo')

parser.add_argument("-out", help="out fifo addr interepter", default='in.fifo')

parser.add_argument("-ip", help="ip rcp openOCD server", default="127.0.0.1")

parser.add_argument("-port", help="port rcp openOCD server", type=int, default=6666)

args = parser.parse_args()

print(args.in_, args.out, args.ip)

IN_FIFO = args.in_
OUT_FIFO = args.out

try:
    os.mkfifo(IN_FIFO)
except OSError as oe:
    if oe.errno != errno.EEXIST:
        raise

try:
    os.mkfifo(OUT_FIFO)
except OSError as oe:
    if oe.errno != errno.EEXIST:
        raise

if not stat.S_ISFIFO(os.stat(IN_FIFO).st_mode) :
    print(IN_FIFO, "is not named pipe")

if not stat.S_ISFIFO(os.stat(OUT_FIFO).st_mode) :
    print(OUT_FIFO, "is not named pipe")

with open(IN_FIFO,'r') as inFifo:
    with open(OUT_FIFO,'w') as outFifo:
        print("FIFO opened")
        with OpenOcd(tclRpcIp = args.ip) as ocd:
            initOCD(ocd)
            print("OpenOcd init")
            while True:
                line = inFifo.readline()
                if len(line) == 0:
                    print(IN_FIFO," closed")
                    break
                print("Get line:",line)
                answer = proccess(line)
                print("answer : ", answer)
                outFifo.write(answer)
                outFifo.flush()

