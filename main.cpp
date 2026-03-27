#include <SFML/Graphics.hpp>
#include <omp.h>
#include <mpi.h>
#include <CL/cl.h>
#include <iostream>

int main()
{
    std::cout << "OpenMP threads: " << omp_get_max_threads() << std::endl;
    std::cout << "All libraries loaded successfully!" << std::endl;

    sf::RenderWindow window(sf::VideoMode({ 800, 600 }), "DeadLock - Setup Test");

    while (window.isOpen())
    {
        while (const std::optional event = window.pollEvent())
        {
            if (event->is<sf::Event::Closed>())
                window.close();
        }

        window.clear(sf::Color::Black);
        window.display();
    }

    return 0;
}