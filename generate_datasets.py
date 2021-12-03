import numpy as npy
import os

def generate_dataset(n_of_dataset: int, srv_no: int):
    mask = 0xFFFF
    mu = mask >> 1
    sigma = 1024 * 1
    s = npy.random.normal(mu, sigma, n_of_dataset)
    s = s.astype(int)
    s = npy.asarray([e % mask for e in s])
    filename = r'./datasets/dataset_' + str(srv_no) + '.txt'
    s.tofile(filename, '\t', "%s")

if __name__ == '__main__':
    os.system(r'rm datasets/* -rf')
    npy.random.seed(0)
    server_number = 1000
    base_n = 1000
    rng = base_n / 10
    for srv_no in range(server_number):
        n_of_dataset = npy.random.randint(base_n-rng, base_n+rng)
        generate_dataset(n_of_dataset, srv_no)
