# Mini Shell
Mini Shell is a lightweight Unix shell implemented in C as part of an academic assignment. The project aims to replicate fundamental shell functionalities, such as executing commands, handling input/output redirections, supporting pipelines, and managing background processes.

## Features
- Command Execution – Runs external programs using execvp().
- Built-in Commands – Implements essential commands like cd, exit, and possibly more.
- Redirections – Supports:
  - Input (<) – Redirects input from a file.
  - Output (> and >>) – Redirects output to a file (overwrite or append).
- Pipes (|) – Supports command chaining with multiple pipes.
- Background Execution (&) – Runs processes asynchronously.
