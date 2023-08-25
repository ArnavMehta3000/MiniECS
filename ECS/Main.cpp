#include "ECS.h"
#include <iostream>

struct Vector
{
    float x;
    float y;
    float z;
};

struct Quaternion
{
    float x;
    float y;
    float z;
    float w;
};

struct Transform
{
    Transform()
    {
        position.x = 0.0f;
        position.y = 0.0f;
        position.z = 0.0f;

        rotation.x = 0.0f;
        rotation.y = 0.0f;
        rotation.z = 0.0f;
        rotation.w = 0.0f;

        scale.x = 0.0f;
        scale.y = 0.0f;
        scale.z = 0.0f;
    }
    Transform(float z)
    {
        position.x = z;
        position.y = z;
        position.z = z;
    }

    Vector position;
    Quaternion rotation;
    Vector scale;
};

struct Shape
{
    bool shape;
};

struct Renderable
{
    bool renderable;
};

int main()
{

    ECS::World r;
    auto e1 = r.NewEntity();
    r.Assign<Transform>(e1, 1.0f);

    auto e2 = r.NewEntity();
    r.Assign<Transform>(e2, 2.0f);
    r.Assign<Shape>(e2);

    auto e3 = r.NewEntity();
    r.Assign<Transform>(e3, 3.0f);
    r.Assign<Shape>(e3);
    r.Assign<Renderable>(e3);

    // Returns a view with all entities with a Transform & Shape component
    for (auto entity : ECS::RosterView<Transform, Shape>(r))
    {
        auto tf = r.Get<Transform>(entity);
        std::cout << tf->position.x << std::endl;
        tf->position.x = 10.0f;
    }

    /*
    * The above loop should print the transform.position.x component of 'e2' and 'e3'
    * Then change it to 10.0
    * And will print:
    * 2.0
    * 3.0
    */


    std::cout << "-----" << std::endl;

    for (auto entity : ECS::RosterView<Transform>(r))
    {
        auto transform = r.Get<Transform>(entity);
        std::cout << transform->position.x << std::endl;
    }

    /*
    * The above loop should print the transform.position.x component of 'e1', 'e2' and 'e3'
    * And will print:
    * 1.0
    * 10.0
    * 10.0
    */

    std::cout << "-----" << std::endl;

    if (r.Has<Transform>(e1))
        std::cout << "e1 has transform" << std::endl;


    // Destroys entity 'e2' and removes transform from entity 'e3'
    r.DestroyEntity(e2);
    r.Remove<Transform>(e3);

    for (auto entity : ECS::RosterView<Transform>(r))
    {
        auto transform = r.Get<Transform>(entity);
        std::cout << transform->position.x << std::endl;
    }


    return 0;
}