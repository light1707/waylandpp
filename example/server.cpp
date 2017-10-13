#include <wayland-server-protocol.hpp>
#include <iostream>
#include <sys/mman.h>

using namespace wayland::server;

// A rectangular area
struct rect_t
{
  int32_t x;
  int32_t y;
  int32_t w;
  int32_t h;
  bool operator==(const rect_t &rect)
  {
    return x == rect.x && y == rect.y && w == rect.w && h == rect.h;
  }
};

// internal data of a buffer
struct buffer_data
{
  shm_pool_t shm_pool;
  int32_t offset;
  int32_t width;
  int32_t height;
  int32_t stride;
  shm_format format;
};

// internat data of a surface
struct surface_data
{
  buffer_t buffer;
  int32_t offx;
  int32_t offy;
  rect_t damage;
  region_t opaque;
  region_t input;
};

// internat data of a shm pool
struct shm_pool_data
{
  int fd;
  int32_t size;
};

// helper to create a std::function out of a member function and an object
template <typename R, typename T, typename... Args>
std::function<R(Args...)> bind_mem_fn(R(T::* func)(Args...), T *t)
{
  return [func, t] (Args... args)
    {
      return (t->*func)(args...);
    };
}

class server
{
private:
  display_t display;
  global_seat_t seat;
  global_output_t output;
  global_shm_t shm;
  global_compositor_t compositor;
  global_shell_t shell;
  std::list<callback_t> callbacks;

  void compositor_bind(client_t client, compositor_t compositor)
  {
    // surfaces
    compositor.on_create_surface() = [this] (surface_t surface)
      {
        surface.user_data() = surface_data();
        surface.on_attach() = [surface] (buffer_t buffer, int32_t offx, int32_t offy) mutable
        {
          surface.user_data().get<surface_data>().buffer = buffer;
        };
        surface.on_damage() = [surface] (int32_t x, int32_t y, int32_t w, int32_t h) mutable
        {
          rect_t rect = surface.user_data().get<surface_data>().damage;
          rect_t result;
          if(rect == rect_t{0, 0, 0, 0})
            result = {x, y, w, h};
          else
            { // naive AND
              result.x = x < rect.x ? x : rect.x;
              result.y = y < rect.y ? y : rect.y;
              result.w = x+w > rect.x+rect.w ? x+w-result.x : rect.x+rect.w-result.x;
              result.h = x+h > rect.x+rect.h ? x+h-result.x : rect.x+rect.h-result.x;
            }
          surface.user_data().get<surface_data>().damage = result;
        };
        surface.on_commit() = [this, surface] () mutable
        {
          // draw here
          surface.user_data().get<surface_data>().damage = rect_t{0, 0, 0, 0};
        };
        surface.on_frame() = [this] (callback_t callback)
        {
          callbacks.push_back(callback);
        };
        surface.on_set_opaque_region() = [surface] (region_t region) mutable
        {
          surface.user_data().get<surface_data>().opaque = region;
        };
        surface.on_set_input_region() = [surface] (region_t region) mutable
        {
          surface.user_data().get<surface_data>().input = region;
        };
      };

    // regions
    compositor.on_create_region() = [this] (region_t region)
      {
        region.on_add() = [region] (int32_t x, int32_t y, int32_t w, int32_t h)
        {
          // TODO: add to region
        };
        region.on_subtract() = [region] (int32_t x, int32_t y, int32_t w, int32_t h)
        {
          // TODO: subtract from region
        };
      };
  }

public:
  server()
    : seat(display), output(display), shm(display), compositor(display), shell(display)
  {
    // create wayland-0 UNIX socket
    display.add_socket_auto();

    // Announce seat capabilities
    seat.on_bind() = [] (client_t client, seat_t seat)
      {
        seat.capabilities(seat_capability::keyboard | seat_capability::pointer | seat_capability::touch);
      };

    // Announce output properties
    output.on_bind() = [this] (client_t client, output_t output)
      {
        output.geometry(1024, 748, 400, 300, output_subpixel::horizontal_rgb, "Make", "Model", output_transform::normal);
        output.mode(output_mode::current, 1024, 768, 60000);
        output.scale(1);
        output.done();
      };

    // Shared memory support
    shm.on_bind() = [] (client_t client, shm_t shm)
      {
        // Announce SHM formats
        shm.format(shm_format::argb8888);
        shm.format(shm_format::xrgb8888);
        // Save SHM pools
        shm.on_create_pool() = [] (shm_pool_t shm_pool, int fd, int32_t size)
        {
          shm_pool.user_data() = shm_pool_data{fd, size};
          // Save SHM buffers
          shm_pool.on_create_buffer() = [shm_pool] (buffer_t buffer, int32_t offset, int32_t width, int32_t height, int32_t stride, shm_format format) mutable
          {
            buffer.user_data() = buffer_data{shm_pool, offset, width, height, stride, format};
            buffer.on_destroy() = [buffer] () mutable
            {
              buffer.release();
            };
          };
          shm_pool.on_resize() = [shm_pool] (int32_t size) mutable
          {
            shm_pool.user_data().get<shm_pool_data>().size = size;
          };
        };
      };

    // Handle surfaces and regions
    compositor.on_bind() = bind_mem_fn(&server::compositor_bind, this);
  }

  // Main event loop
  void run()
  {
    event_loop_t el = display.get_event_loop();
    for(uint32_t time = 0; true; time++)
      {
        el.dispatch(1);
        display.flush_clients();
        while(callbacks.size())
          {
            callbacks.front().done(time);
            callbacks.front().destroy();
            callbacks.pop_front();
          }
      }
  }
};

int main()
{
  server display;
  display.run();
  return 0;
}
