Testing:
mkdir -p ../traces
for z in *_four.zip; do
  echo "Extracting $z..."
  unzip -jo "$z" '*.data' -x "__MACOSX/*" -d ../traces
done 

make

cd scripts
bash run_part1.sh
bash sweep_part1.sh




