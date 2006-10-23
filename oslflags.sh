#!/bin/sh
case `uname` in
  Darwin) echo -Wl,-pagezero_size,08000000 ;;
esac
