import numpy as npy
import os

def generate_dataset(n_of_dataset: int, srv_no: int, server_number: int):
    mask = 0xFFF
    mu = mask * srv_no / server_number
    sigma = 256
    s = npy.random.normal(mu, sigma, n_of_dataset)
    s = s.astype(int)
    s = npy.asarray([e % mask for e in s])
    filename = r'./datasets/dataset_' + str(srv_no) + '.txt'
    s.tofile(filename, '\t', "%s")

if __name__ == '__main__':
    os.system(r'rm datasets/* -rf')
    npy.random.seed(0)
    server_number = 10000
    n_of_dataset = 1024
    for srv_no in range(server_number):
        generate_dataset(n_of_dataset, srv_no, server_number)
