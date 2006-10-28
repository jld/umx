#!/bin/sh
case `uname` in
  Darwin) echo -mdynamic-no-pic ;;
esac
