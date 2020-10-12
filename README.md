### Conda Setup
```bash
conda env create --file environment.yml
conda env create --file environment.yml --force  # if env already exists
conda activate zmq-eval

conda deactivate
conda env remove -n zmq-eval
```

```bash
mkdir build && cd build
conan install .. --build missing
cmake ..
cmake --build . -j $(nproc)
```
