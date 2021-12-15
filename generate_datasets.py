import scipy.stats as stats
import numpy as npy
import os

def generate_dataset(n_of_dataset: int, srv_no: int, hf_ratio: float):
    filename = r'./datasets/dataset_' + str(srv_no) + '.txt'

    set1_mask = 0xFFFF
    set1_size = int(n_of_dataset * hf_ratio)
    unif = npy.random.uniform(0, set1_mask, set1_size)
    unif = [int(e) for e in unif]

    set2_size = n_of_dataset - set1_size
    for i in range(1, set2_size):
        addition = (i + srv_no) % set2_size
        unif.append(set1_mask + addition)

    unif = npy.asarray(unif)
    unif.tofile(filename, "\t", "%s")


if __name__ == '__main__':
    os.system(r'rm datasets/* -rf')
    npy.random.seed(0)
    server_number = 1000
    base_n = 1024
    rng = 0
    hf_ratio = float(input())
    for srv_no in range(server_number):
        n_of_dataset = base_n if rng == 0 else npy.random.randint(base_n-rng, base_n+rng)
        generate_dataset(n_of_dataset, srv_no, hf_ratio)
