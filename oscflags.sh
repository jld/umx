#!/bin/sh
case `uname` in
  Darwin) echo -mdynamic-no-pic ;;
esac
case `uname -m` in
  *64) echo -m32 ;;
esac
