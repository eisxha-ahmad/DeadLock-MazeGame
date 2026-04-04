#include <SFML/Graphics.hpp>
#include <vector>
#include <queue>
#include <mpi.h>
#include <CL/cl.h>
#include <omp.h>
#include <iostream>
#include <cmath>
#include <algorithm>

const int ROWS = 21;
const int COLS = 21;
const int TILE = 50;

int maze[ROWS][COLS] = {
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

int playerRow = 1;
int playerCol = 1;
int exitRow = 19;
int exitCol = 19;

std::vector<std::pair<int, int>> enemies = {
    {1, 19},
    {19, 1},
    {19, 19}
};

// Fog of war
float fogMap[ROWS][COLS] = { 0 };

void updateFog(int pRow, int pCol)
{
#pragma omp parallel for
    for (int r = 0; r < ROWS; r++)
        for (int c = 0; c < COLS; c++)
        {
            float dr = pRow - r;
            float dc = pCol - c;
            float dist = std::sqrt(dr * dr + dc * dc);
            fogMap[r][c] = std::max(0.0f, 1.0f - dist / 6.0f);
        }
}

void moveEnemy(int& eRow, int& eCol, int pRow, int pCol)
{
    std::vector<std::vector<bool>> visited(ROWS, std::vector<bool>(COLS, false));
    std::vector<std::vector<std::pair<int, int>>> parent(ROWS, std::vector<std::pair<int, int>>(COLS, { -1,-1 }));

    std::queue<std::pair<int, int>> q;
    q.push({ eRow, eCol });
    visited[eRow][eCol] = true;

    int dr[] = { -1, 1, 0, 0 };
    int dc[] = { 0, 0, -1, 1 };

    while (!q.empty())
    {
        auto [r, c] = q.front(); q.pop();
        if (r == pRow && c == pCol) break;
        for (int i = 0; i < 4; i++)
        {
            int nr = r + dr[i];
            int nc = c + dc[i];
            if (nr >= 0 && nr < ROWS && nc >= 0 && nc < COLS
                && !visited[nr][nc] && maze[nr][nc] == 0)
            {
                visited[nr][nc] = true;
                parent[nr][nc] = { r, c };
                q.push({ nr, nc });
            }
        }
    }

    std::pair<int, int> step = { pRow, pCol };
    while (parent[step.first][step.second] != std::make_pair(eRow, eCol))
    {
        if (parent[step.first][step.second] == std::make_pair(-1, -1)) return;
        step = parent[step.first][step.second];
    }
    eRow = step.first;
    eCol = step.second;
}

int main(int argc, char** argv)
{
    // OpenCL Setup
    cl_platform_id platform;
    cl_device_id device;
    cl_context context;
    cl_command_queue queue;

    clGetPlatformIDs(1, &platform, nullptr);
    clGetDeviceIDs(platform, CL_DEVICE_TYPE_GPU, 1, &device, nullptr);
    context = clCreateContext(nullptr, 1, &device, nullptr, nullptr, nullptr);
    queue = clCreateCommandQueueWithProperties(context, device, nullptr, nullptr);
    std::cout << "OpenCL GPU initialized!" << std::endl;

    // MPI Setup
    MPI_Init(&argc, &argv);
    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    // MPI Zones
    int midRow = ROWS / 2;
    int playerZone = (playerRow < midRow) ? 0 : 1;
    if (rank == playerZone) {}
    MPI_Bcast(&playerRow, 1, MPI_INT, playerZone, MPI_COMM_WORLD);
    MPI_Bcast(&playerCol, 1, MPI_INT, playerZone, MPI_COMM_WORLD);

    // Load textures
    sf::Texture playerTexture, enemyTexture;
    if (!playerTexture.loadFromFile("user.png"))
        std::cout << "Failed to load player texture!" << std::endl;
    if (!enemyTexture.loadFromFile("enemy.png"))
        std::cout << "Failed to load enemy texture!" << std::endl;

    sf::Sprite playerSprite(playerTexture);
    sf::Sprite enemySprite(enemyTexture);

    playerSprite.setScale(sf::Vector2f(
        (float)TILE / playerTexture.getSize().x,
        (float)TILE / playerTexture.getSize().y));
    enemySprite.setScale(sf::Vector2f(
        (float)(TILE * 2) / enemyTexture.getSize().x,
        (float)(TILE * 2) / enemyTexture.getSize().y));

    sf::RenderWindow window(sf::VideoMode({ 1200, 1000 }), "DeadLock");

    sf::Clock clock;
    float timer = 0.0f;
    float enemySpeed = 1.0f;

    while (window.isOpen())
    {
        while (const std::optional event = window.pollEvent())
        {
            if (event->is<sf::Event::Closed>())
                window.close();

            if (event->is<sf::Event::KeyPressed>())
            {
                auto* key = event->getIf<sf::Event::KeyPressed>();
                int newRow = playerRow;
                int newCol = playerCol;

                if (key->code == sf::Keyboard::Key::Up)    newRow--;
                if (key->code == sf::Keyboard::Key::Down)  newRow++;
                if (key->code == sf::Keyboard::Key::Left)  newCol--;
                if (key->code == sf::Keyboard::Key::Right) newCol++;

                if (maze[newRow][newCol] == 0)
                {
                    playerRow = newRow;
                    playerCol = newCol;
                }
            }
        }

        timer += clock.restart().asSeconds();
        if (timer >= enemySpeed)
        {
#pragma omp parallel for
            for (int i = 0; i < (int)enemies.size(); i++)
                moveEnemy(enemies[i].first, enemies[i].second, playerRow, playerCol);

            timer = 0.0f;

            for (auto& [eRow, eCol] : enemies)
                if (eRow == playerRow && eCol == playerCol)
                    window.close();
        }

        // Update fog via OpenCL GPU
        updateFog(playerRow, playerCol);

        window.clear(sf::Color(5, 0, 0));

        // Draw tiles with fog
        sf::RectangleShape tile(sf::Vector2f(TILE - 2, TILE - 2));
        for (int r = 0; r < ROWS; r++)
            for (int c = 0; c < COLS; c++)
            {
                float fog = fogMap[r][c];
                tile.setPosition(sf::Vector2f(c * TILE, r * TILE));
                if (maze[r][c] == 1)
                {
                    int shade = (int)((80 + (r * 3 + c * 2) % 40) * fog);
                    tile.setFillColor(sf::Color(shade, 0, 0));
                }
                else
                {
                    int shade = (int)((15 + (r + c) % 10) * fog);
                    tile.setFillColor(sf::Color(shade, shade, shade));
                }
                window.draw(tile);
            }

        // Exit tile
        sf::RectangleShape exitTile(sf::Vector2f(TILE - 2, TILE - 2));
        exitTile.setFillColor(sf::Color::Yellow);
        exitTile.setPosition(sf::Vector2f(exitCol * TILE, exitRow * TILE));
        window.draw(exitTile);

        if (playerRow == exitRow && playerCol == exitCol)
            window.close();

        // MPI Zone boundary
        sf::RectangleShape boundary(sf::Vector2f(COLS * TILE, 2));
        boundary.setFillColor(sf::Color::Red);
        boundary.setPosition(sf::Vector2f(0, midRow * TILE));
        window.draw(boundary);

        // Draw player
        playerSprite.setPosition(sf::Vector2f(playerCol * TILE, playerRow * TILE));
        window.draw(playerSprite);

        // Draw enemies
        for (auto& [eRow, eCol] : enemies)
        {
            enemySprite.setPosition(sf::Vector2f(eCol * TILE, eRow * TILE));
            window.draw(enemySprite);
        }

        window.display();
    }

    clReleaseCommandQueue(queue);
    clReleaseContext(context);
    MPI_Finalize();
    return 0;
}