#!/bin/bash
# Wrapper script to run blocktest with xvfb-run for debugging
exec xvfb-run "$@"