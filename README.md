# Distributed Matrix Multiplication

This project implements a distributed matrix multiplication system using a master-slave architecture.
The master server coordinates multiple client nodes to perform parallel matrix multiplication.

## Features

- Dynamic workload distribution
- SIMD optimized computation at clients
- Runtime client connection/disconnection
- Scalable with number of clients

## Building

```bash
make all