#!/bin/bash
set -x -e

sleep 1

export DISABLE_SPRING=true

# Setup DB
./bin/rails db:reset

# Setup statistics
./bin/rails statistics:setup

# Run tests
./bin/rails test

# Run system tests
./bin/rails test:system