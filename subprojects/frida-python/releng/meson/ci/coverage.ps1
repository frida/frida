echo ""
echo ""
echo "=== Gathering coverage report ==="
echo ""

python3 -m coverage combine
python3 -m coverage xml
python3 -m coverage report

# Currently codecov.py does not handle Azure, use this fork of a fork to get it
# working without requiring a token
git clone https://github.com/mensinda/codecov-python
python3 -m pip install --ignore-installed ./codecov-python
python3 -m codecov -f .coverage/coverage.xml -n "VS$env:compiler $env:arch $env:backend" -c $env:SOURCE_VERSION
