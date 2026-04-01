
#include <SFML/Graphics.hpp>
#include <mpi.h>
#include <omp.h>
#include <CL/cl.h>
#include <iostream>
#include <cmath>
#include <vector>

// Constants
const int SCREEN_W = 800;
const int SCREEN_H = 600;
const int MAP_W = 21;
const int MAP_H = 21;
const float FOV = 3.14159f / 3.0f; // 60 degrees
const int NUM_RAYS = SCREEN_W;

int maze[MAP_H][MAP_W] = {
    {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
    {1,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,1},
    {1,0,1,1,0,1,1,1,0,1,1,1,0,1,1,1,0,1,1,0,1},
    {1,0,1,0,0,0,0,1,0,0,0,0,0,1,0,0,0,0,1,0,1},
    {1,0,0,0,1,0,0,0,0,1,1,1,0,0,0,1,0,0,0,0,1},
    {1,1,1,0,1,1,0,1,1,1,0,1,1,1,0,1,1,0,1,1,1},
    {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
    {1,0,1,1,0,1,0,1,1,0,1,0,1,1,0,1,0,1,1,0,1},
    {1,0,0,1,0,1,0,0,1,0,1,0,1,0,0,1,0,1,0,0,1},
    {1,1,0,1,0,0,0,0,0,0,1,0,0,0,0,0,0,1,0,1,1},
    {1,0,0,0,0,1,0,1,0,0,0,0,0,1,0,1,0,0,0,0,1},
    {1,1,0,1,0,0,0,0,0,0,1,0,0,0,0,0,0,1,0,1,1},
    {1,0,0,1,0,1,0,1,1,0,1,0,1,1,0,1,0,1,0,0,1},
    {1,0,1,1,0,1,0,1,1,0,1,0,1,1,0,1,0,1,1,0,1},
    {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
    {1,1,1,0,1,1,0,1,1,1,0,1,1,1,0,1,1,0,1,1,1},
    {1,0,0,0,1,0,0,0,0,1,1,1,0,0,0,1,0,0,0,0,1},
    {1,0,1,0,0,0,0,1,0,0,0,0,0,1,0,0,0,0,1,0,1},
    {1,0,1,1,0,1,1,1,0,1,1,1,0,1,1,1,0,1,1,0,1},
    {1,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,1},
    {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
};

// Player
float playerX = 1.5f;
float playerY = 1.5f;
float playerAngle = 0.0f;
const float MOVE_SPEED = 0.05f;
const float ROT_SPEED = 0.03f;

// Raycasting
struct Ray {
    float distance;
    int wallType;
};

Ray castRay(float angle)
{
    float rayX = playerX;
    float rayY = playerY;
    float rayDirX = std::cos(angle);
    float rayDirY = std::sin(angle);

    for (int i = 0; i < 100; i++)
    {
        rayX += rayDirX * 0.05f;
        rayY += rayDirY * 0.05f;

        int mapX = (int)rayX;
        int mapY = (int)rayY;

        if (mapX < 0 || mapX >= MAP_W || mapY < 0 || mapY >= MAP_H)
            return { 100.0f, 0 };

        if (maze[mapY][mapX] == 1)
        {
            float dx = rayX - playerX;
            float dy = rayY - playerY;
            float dist = std::sqrt(dx * dx + dy * dy);
            return { dist, 1 };
        }
    }
    return { 100.0f, 0 };
}

int main(int argc, char** argv)
{
    MPI_Init(&argc, &argv);
    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    // OpenCL setup
    cl_platform_id platform;
    cl_device_id device;
    cl_context context;
    cl_command_queue queue;
    clGetPlatformIDs(1, &platform, nullptr);
    clGetDeviceIDs(platform, CL_DEVICE_TYPE_GPU, 1, &device, nullptr);
    context = clCreateContext(nullptr, 1, &device, nullptr, nullptr, nullptr);
    queue = clCreateCommandQueueWithProperties(context, device, nullptr, nullptr);

    sf::RenderWindow window(sf::VideoMode({ SCREEN_W, SCREEN_H }), "DeadLock 3D");

    std::vector<Ray> rays(NUM_RAYS);

    while (window.isOpen())
    {
        while (const std::optional event = window.pollEvent())
            if (event->is<sf::Event::Closed>())
                window.close();

        // Player movement
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Left))
            playerAngle -= ROT_SPEED;
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Right))
            playerAngle += ROT_SPEED;
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Up))
        {
            float nx = playerX + std::cos(playerAngle) * MOVE_SPEED;
            float ny = playerY + std::sin(playerAngle) * MOVE_SPEED;
            if (maze[(int)ny][(int)nx] == 0) { playerX = nx; playerY = ny; }
        }
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Down))
        {
            float nx = playerX - std::cos(playerAngle) * MOVE_SPEED;
            float ny = playerY - std::sin(playerAngle) * MOVE_SPEED;
            if (maze[(int)ny][(int)nx] == 0) { playerX = nx; playerY = ny; }
        }

        // Cast rays using OpenMP
#pragma omp parallel for
        for (int i = 0; i < NUM_RAYS; i++)
        {
            float angle = playerAngle - FOV / 2 + (float)i / NUM_RAYS * FOV;
            rays[i] = castRay(angle);
        }

        // Render
        window.clear(sf::Color::Black);

        // Floor and ceiling
        sf::RectangleShape ceiling(sf::Vector2f(SCREEN_W, SCREEN_H / 2));
        ceiling.setFillColor(sf::Color(20, 0, 0));
        window.draw(ceiling);

        sf::RectangleShape floor(sf::Vector2f(SCREEN_W, SCREEN_H / 2));
        floor.setPosition(sf::Vector2f(0, SCREEN_H / 2));
        floor.setFillColor(sf::Color(10, 10, 10));
        window.draw(floor);

        // Walls
        for (int i = 0; i < NUM_RAYS; i++)
        {
            float dist = rays[i].distance;
            int wallH = (int)(SCREEN_H / dist);

            int shade = std::min(255, (int)(255 / (dist + 1)));
            sf::RectangleShape wall(sf::Vector2f(1, wallH));
            wall.setPosition(sf::Vector2f(i, (SCREEN_H - wallH) / 2));
            wall.setFillColor(sf::Color(shade, 0, 0));
            window.draw(wall);
        }

        window.display();
    }

    clReleaseCommandQueue(queue);
    clReleaseContext(context);
    MPI_Finalize();
    return 0;
}
