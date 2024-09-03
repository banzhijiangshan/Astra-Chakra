#!/bin/sh

size=8

while [ $size -le 2147483648 ]; do
  mkdir -p "size_$size"
  echo "Created directory: size_$size"
  
  cd "size_$size"
  python3 -m chakra.et_generator.et_generator --num_npus 4 --num_dims 1 --default_comm_size $size
  cd ..

  size=$(( size * 2 ))
done

echo "All directories created successfully."
