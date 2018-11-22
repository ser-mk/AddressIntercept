#!/usr/bin/env python3.5m
# -*- coding: utf-8 -*-

import sys, os, time
import errno
import stat
import argparse
import logging

from ocd_rpc import OpenOcd

from addrIntercept import proccess, initOCD


parser = argparse.ArgumentParser(description='OCD client')

parser.add_argument("-in_", help="in fifo  addr interepter", default='out.fifo')

parser.add_argument("-out", help="out fifo addr interepter", default='in.fifo')

parser.add_argument("-ip", help="ip rcp openOCD server", default="127.0.0.1")

parser.add_argument("-port", help="port rcp openOCD server", type=int, default=6666)

parser.add_argument("-v", help="verbosity level log: DEBUG, INFO, WARNING, ERROR", default="ERROR")

args = parser.parse_args()

print(args.in_, args.out, args.ip, args.v)

logging.basicConfig( format = u'%(filename)s[LINE:%(lineno)d]* %(levelname)-8s [%(asctime)s]  %(message)s',
    level = args.v)

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
    logging.error("is not named pipe: %s", IN_FIFO)

if not stat.S_ISFIFO(os.stat(OUT_FIFO).st_mode) :
    logging.error("is not named pipe: %s", OUT_FIFO)

with open(IN_FIFO,'r') as inFifo:
    with open(OUT_FIFO,'w') as outFifo:
        logging.info("FIFO opened")
        with OpenOcd(verbose = args.v == "DEBUG", tclRpcIp = args.ip) as ocd:
            initOCD(ocd)
            logging.info("OpenOcd init")
            while True:
                line = inFifo.readline()
                if len(line) == 0:
                    logging.warning("closed fifo %s", IN_FIFO)
                    break
                logging.debug("Get line: %s",line)
                answer = proccess(line)
                logging.debug("answer : %s", answer)
                outFifo.write(answer)
                outFifo.flush()

