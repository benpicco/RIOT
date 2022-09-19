#!/usr/bin/env python3

import sys
from testrunner import run


def testfunc(child):
    child.expect("START LIST")
    child.expect_exact("J")
    child.expect_exact("I")
    child.expect_exact("H")
    child.expect_exact("G")
    child.expect_exact("F")
    child.expect_exact("E")
    child.expect_exact("D")
    child.expect_exact("C")
    child.expect_exact("END LIST")

    child.expect("START LIST")
    child.expect_exact("I")
    child.expect_exact("H")
    child.expect_exact("G")
    child.expect_exact("E")
    child.expect_exact("D")
    child.expect_exact("[empty]")
    child.expect_exact("[empty]")
    child.expect_exact("[empty]")
    child.expect_exact("END LIST")

    child.expect("START LIST")
    child.expect_exact("G")
    child.expect_exact("D")
    child.expect_exact("I")
    child.expect_exact("H")
    child.expect_exact("E")
    child.expect_exact("[empty]")
    child.expect_exact("[empty]")
    child.expect_exact("[empty]")
    child.expect_exact("END LIST")


if __name__ == "__main__":
    sys.exit(run(testfunc))
