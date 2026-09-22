#!/bin/bash

echo "Combining coverage reports..."
coverage combine

echo "Generating XML report..."
coverage xml

echo "Printing report"
coverage report
