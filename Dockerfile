# Use the official GCC image as a parent image
FROM gcc:latest

# Set the working directory in the container
WORKDIR /usr/src/app

# Install CMake
RUN apt-get update && \
    apt-get install -y cmake && \
    rm -rf /var/lib/apt/lists/*

# Copy the current directory contents into the container at /usr/src/app
COPY . .

# Configure the build with CMake
RUN cmake -B build -S .

# Build the project
RUN cmake --build build

# Make port 8080 available to the world outside this container
EXPOSE 8080

# Run the executable
CMD ["./build/http_server", "8080"]
