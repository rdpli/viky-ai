#!/bin/bash
set -x -e

# remove previously started server pid
rm -f ./tmp/pids/server.pid

# Setup DB
./bin/rails db:create db:migrate

# Start web server
./bin/rails server -b 0.0.0.0 -p 3000
