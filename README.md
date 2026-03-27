# Parallel Matrix Multiplication using OS Concepts

This project implements parallel matrix multiplication using core Operating System concepts like process creation, inter-process communication (IPC), and synchronization. The goal was to understand how multiple processes can work together efficiently to solve a computationally intensive problem.

---

## 💡 Overview

The system follows a **master-worker architecture**:

* The **master process** divides matrix multiplication into smaller tasks
* These tasks are distributed to **worker processes**
* Workers compute assigned rows and send results back
* Progress is tracked in real-time and displayed through a simple web-based interface

---

## 🧠 Concepts Used

* **Process Management** → multiple worker processes
* **Message Queues** → for task distribution
* **Shared Memory** → for storing matrices
* **Semaphores** → for synchronization and safe access
* **Parallel Processing** → improves computation efficiency

---

## 🚀 Features

* Parallel computation of matrix multiplication
* Efficient task distribution using message queues
* Shared memory for fast data access
* Synchronization using semaphores
* Dynamic load balancing across workers
* Real-time progress tracking
* Web-based GUI for visualization

---

## 🛠 Technologies Used

* C++ (System Programming)
* Linux System V IPC
* HTML, CSS, JavaScript (Frontend GUI)

---

## ▶️ How to Run

### 1. Compile the program

```
g++ -std=c++11 src/master.cpp -o master
```

### 2. Run the program

```
./master <matrix_size> <workers>
```

Example:

```
./master 300 4
```

---

## 🌐 GUI

To view progress and results:

* Open `gui/index.html` in your browser
* It will display:

  * Computation progress
  * Matrix preview
  * Final results

---

## 📂 Project Structure

* `src/master.cpp` → main logic and process coordination
* `gui/index.html` → frontend dashboard
* `gui/app.js` → handles data updates
* `gui/style.css` → UI styling
* `progress.json` → real-time progress tracking
* `results.json` → final output

---

## 🎯 Purpose

This project was developed as part of an Operating Systems course to understand how real-world systems handle parallel processing, synchronization, and communication between processes.

---

## 🔮 Future Improvements

* Add support for larger datasets
* Improve visualization and UI
* Extend to distributed systems
* Optimize performance further
