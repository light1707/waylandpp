/*
 * Copyright (c) 2017, Nils Christopher Brause
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdexcept>
#include <iostream>
#include <wayland-server.hpp>

using namespace wayland::server;
using namespace wayland::server::detail;
using namespace wayland::detail;

//-----------------------------------------------------------------------------

display_t::data_t *display_t::wl_display_get_user_data(wl_display *display)
{
  wl_listener *listener = wl_display_get_destroy_listener(display, destroy_func);
  if(listener)
    return reinterpret_cast<data_t*>(reinterpret_cast<listener_t*>(listener)->user);
  else
    return NULL;
}

void display_t::destroy_func(wl_listener *listener, void *)
{
  display_t::data_t *data = reinterpret_cast<display_t::data_t*>(reinterpret_cast<listener_t*>(listener)->user);
  if(data->destroy)
    data->destroy();
}

void display_t::client_created_func(wl_listener *listener, void *cl)
{
  display_t::data_t *data = reinterpret_cast<display_t::data_t*>(reinterpret_cast<listener_t*>(listener)->user);
  client_t client(reinterpret_cast<wl_client*>(cl));
  if(data->client_created)
    data->client_created(client);
}

void display_t::init()
{
  data = new data_t;
  data->counter = 1;
  data->destroy_listener.user = data;
  data->client_created_listener.user = data;
  data->destroy_listener.listener.notify = destroy_func;
  data->client_created_listener.listener.notify = client_created_func;
  wl_display_add_destroy_listener(display, reinterpret_cast<wl_listener*>(&data->destroy_listener));
  wl_display_add_client_created_listener(display, reinterpret_cast<wl_listener*>(&data->client_created_listener));
}

void display_t::fini()
{
  data->counter--;
  if(data->counter == 0)
    {
      delete data;
      wl_display_destroy(c_ptr());
    }
}

display_t::display_t()
{
  display = wl_display_create();
  if(!display)
    throw std::runtime_error("Failed to create display.");
  init();
}

display_t::display_t(wl_display *c)
{
  display = c;
  data = wl_display_get_user_data(c_ptr());
  if(!data)
    init();
  else
    data->counter++;
}

display_t::~display_t()
{
  fini();
}

display_t::display_t(const display_t &p)
{
  display = p.display;
  data = p.data;
  data->counter++;
}

display_t &display_t::operator=(const display_t& p)
{
  fini();
  display = p.display;
  data = p.data;
  data->counter++;
  return *this;
}

bool display_t::operator==(const display_t &d) const
{
  return c_ptr() == d.c_ptr();
}

wl_display *display_t::c_ptr() const
{
  if(!display)
    throw std::runtime_error("display is null.");
  return display;
}

wayland::detail::any &display_t::user_data()
{
  return data->user_data;
}

event_loop_t display_t::get_event_loop()
{
  return wl_display_get_event_loop(c_ptr());
}

int display_t::add_socket(std::string name)
{
  return wl_display_add_socket(c_ptr(), name.c_str());
}

std::string display_t::add_socket_auto()
{
  return wl_display_add_socket_auto(c_ptr());
}

int display_t::add_socket_fd(int sock_fd)
{
  return wl_display_add_socket_fd(c_ptr(), sock_fd);
}

void display_t::terminate()
{
  wl_display_terminate(c_ptr());
}

void display_t::run()
{
  wl_display_run(c_ptr());
}

void display_t::flush_clients()
{
  wl_display_flush_clients(c_ptr());
}

uint32_t display_t::get_serial()
{
  return wl_display_get_serial(c_ptr());
}

uint32_t display_t::next_serial()
{
  return wl_display_next_serial(c_ptr());
}

std::function<void()> &display_t::on_destroy()
{
  return data->destroy;
}

std::function<void(client_t&)> &display_t::on_client_created()
{
  return data->client_created;
}

const std::list<client_t> display_t::get_client_list()
{
  std::list<client_t> clients;
  wl_client *client;
  wl_list *list = wl_display_get_client_list(c_ptr());
  wl_client_for_each(client, list)
    clients.push_back(client_t(client));
  return clients;
}

//-----------------------------------------------------------------------------

client_t::data_t *client_t::wl_client_get_user_data(wl_client *client)
{
  wl_listener *listener = wl_client_get_destroy_listener(client, destroy_func);
  if(listener)
    return reinterpret_cast<data_t*>(reinterpret_cast<listener_t*>(listener)->user);
  else
    return NULL;
}

void client_t::destroy_func(wl_listener *listener, void *)
{
  data_t *data = reinterpret_cast<data_t*>(reinterpret_cast<listener_t*>(listener)->user);
  if(data->destroy)
    data->destroy();
  data->destroyed = true;
  if(data->counter == 0)
    delete data;
}

void client_t::resource_created_func(wl_listener *listener, void *res)
{
  data_t *data = reinterpret_cast<data_t*>(reinterpret_cast<listener_t*>(listener)->user);
  resource_t resource(reinterpret_cast<wl_resource*>(res));
  if(data->resource_created)
    data->resource_created(resource);
}

void client_t::init()
{
  data = new data_t;
  data->client = client;
  data->counter = 1;
  data->destroyed = false;
  data->destroy_listener.user = data;
  data->resource_created_listener.user = data;
  data->destroy_listener.listener.notify = destroy_func;
  data->resource_created_listener.listener.notify = resource_created_func;
  wl_client_add_destroy_listener(client, reinterpret_cast<wl_listener*>(&data->destroy_listener));
  wl_client_add_resource_created_listener(client, reinterpret_cast<wl_listener*>(&data->resource_created_listener));
}

void client_t::fini()
{
  data->counter--;
  if(data->counter == 0 && data->destroyed)
    delete data;
}

client_t::client_t(display_t &d, int fd)
{
  client = wl_client_create(d.display, fd);
  init();
}

client_t::client_t(wl_client *c)
{
  client = c;
  data = wl_client_get_user_data(c_ptr());
  if(!data)
      init();
  else
    data->counter++;
}

client_t::~client_t()
{
  fini();
}

client_t::client_t(const client_t &p)
{
  client = p.client;
  data = p.data;
  data->counter++;
}

client_t &client_t::operator=(const client_t& p)
{
  fini();
  client = p.client;
  data = p.data;
  data->counter++;
  return *this;
}

bool client_t::operator==(const client_t &c) const
{
  return c_ptr() == c.c_ptr();
}

wl_client *client_t::c_ptr() const
{
  if(!client)
    throw std::runtime_error("client is null.");
  return client;
}

wayland::detail::any &client_t::user_data()
{
  return data->user_data;
}

void client_t::destroy()
{
  wl_client_destroy(c_ptr());
}

void client_t::flush()
{
  wl_client_flush(c_ptr());
}

void client_t::get_credentials(pid_t &pid, uid_t &uid, gid_t &gid)
{
  wl_client_get_credentials(c_ptr(), &pid, &uid, &gid);
}

int client_t::get_fd()
{
  return wl_client_get_fd(c_ptr());
}

std::function<void()> &client_t::on_destroy()
{
  return data->destroy;
}

resource_t client_t::get_object(uint32_t id)
{
  wl_resource *resource = wl_client_get_object(client, id);
  if(resource)
    return resource_t(resource);
  else
    return resource_t();
}

void client_t::post_no_memory()
{
  wl_client_post_no_memory(c_ptr());
}

std::function<void(resource_t&)> &client_t::on_resource_created()
{
  return data->resource_created;
}

display_t client_t::get_display()
{
  return wl_client_get_display(c_ptr());
}

enum wl_iterator_result client_t::resource_iterator(struct wl_resource *resource, void *data)
{
  reinterpret_cast<std::list<resource_t>*>(data)->push_back(resource_t(resource));
  return WL_ITERATOR_CONTINUE;
}

std::list<resource_t> client_t::get_resource_list()
{
  std::list<resource_t> resources;
  wl_client_for_each_resource(c_ptr(), resource_iterator, &resources);
  return resources;
}

//-----------------------------------------------------------------------------

void resource_t::destroy_func(wl_listener *listener, void *)
{
  resource_t::data_t *data = reinterpret_cast<resource_t::data_t*>(reinterpret_cast<listener_t*>(listener)->user);
  if(data->destroy)
    data->destroy();
  data->destroyed = true;
  if(data->counter == 0)
    delete data;
}

int resource_t::dummy_dispatcher(int opcode, std::vector<wayland::detail::any> args, std::shared_ptr<resource_t::events_base_t> events)
{
  return 0;
}

void resource_t::init()
{
  data = new data_t;
  data->resource = resource;
  data->counter = 1;
  data->destroyed = false;
  data->destroy_listener.user = data;
  data->destroy_listener.listener.notify = destroy_func;
  wl_resource_set_user_data(resource, data);
  wl_resource_add_destroy_listener(resource, reinterpret_cast<wl_listener*>(&data->destroy_listener));
  wl_resource_set_dispatcher(resource, c_dispatcher, reinterpret_cast<void*>(dummy_dispatcher), data, nullptr); // dummy dispatcher
}

void resource_t::fini()
{
  if(data)
    {
      data->counter--;
      if(data->counter == 0 && data->destroyed)
        delete data;
    }
}

resource_t::resource_t()
{
}

resource_t::resource_t(client_t client, const wl_interface *interface, int version, uint32_t id)
{
  resource = wl_resource_create(client.c_ptr(), interface, version, id);
  init();
}

resource_t::resource_t(wl_resource *c)
{
  resource = c;
  data = static_cast<data_t*>(wl_resource_get_user_data(c_ptr()));
  if(!data)
    init();
  else
    data->counter++;
}

resource_t::~resource_t()
{
  fini();
}

resource_t::resource_t(const resource_t &p)
{
  resource = p.resource;
  data = p.data;
  if(data)
    data->counter++;
}

resource_t &resource_t::operator=(const resource_t& p)
{
  fini();
  resource = p.resource;
  data = p.data;
  if(data)
    data->counter++;
  return *this;
}

bool resource_t::operator==(const resource_t &r) const
{
  return resource == r.resource;
}

resource_t::operator bool() const
{
  return resource != nullptr;
}

wl_resource *resource_t::c_ptr() const
{
  if(!resource)
    throw std::runtime_error("resource is null.");
  return resource;
}

bool resource_t::proxy_has_object() const
{
  return resource;
}

wayland::detail::any &resource_t::user_data()
{
  return data->user_data;
}

void resource_t::destroy()
{
  wl_resource_destroy(c_ptr());
}

int resource_t::c_dispatcher(const void *implementation, void *target, uint32_t opcode, const wl_message *message, wl_argument *args)
{
  if(!implementation)
    throw std::invalid_argument("resource dispatcher: implementation is NULL.");
  if(!target)
    throw std::invalid_argument("resource dispatcher: target is NULL.");
  if(!message)
    throw std::invalid_argument("resource dispatcher: message is NULL.");
  if(!args)
    throw std::invalid_argument("resource dispatcher: args is NULL.");

  resource_t p(reinterpret_cast<wl_resource*>(target));
  client_t cl = p.get_client();

  std::string signature(message->signature);
  std::vector<any> vargs;
  unsigned int c = 0;
  for(char ch : signature)
    {
      if(ch == '?' || isdigit(ch))
        continue;

      any a;
      switch(ch)
        {
          // int_32_t
        case 'i':
        case 'h':
        case 'f':
          a = args[c].i;
          break;
          // uint32_t
        case 'u':
          a = args[c].u;
          break;
          // string
        case 's':
          if(args[c].s)
            a = std::string(args[c].s);
          else
            a = std::string("");
          break;
          // resource
        case 'o':
          if(args[c].o)
            a = resource_t(reinterpret_cast<wl_resource*>(args[c].o));
          else
            a = resource_t();
          break;
          // new id
        case 'n':
          {
            if(args[c].n)
              a = resource_t(cl, message->types[c], message->types[c]->version, args[c].n);
            else
              a = resource_t();
          }
          break;
          // array
        case 'a':
          if(args[c].a)
            a = array_t(args[c].a);
          else
            a = array_t();
          break;
        default:
          a = 0;
          break;
        }
      vargs.push_back(a);
      c++;
    }

  typedef int(*dispatcher_func)(int, std::vector<any>, std::shared_ptr<resource_t::events_base_t>);
  dispatcher_func dispatcher = reinterpret_cast<dispatcher_func>(const_cast<void*>(implementation));
  return dispatcher(opcode, vargs, p.get_events());
}

void resource_t::set_events(std::shared_ptr<events_base_t> events,
                         int(*dispatcher)(int, std::vector<any>, std::shared_ptr<resource_t::events_base_t>))
{
  // set only one time
  if(!data->events)
    {
      data->events = events;
      // the dispatcher gets 'implemetation'
      wl_resource_set_dispatcher(c_ptr(), c_dispatcher, reinterpret_cast<void*>(dispatcher), data, nullptr);
    }
}

std::shared_ptr<resource_t::events_base_t> resource_t::get_events()
{
  return data->events;
}

void resource_t::post_event_array(uint32_t opcode, std::vector<argument_t> v)
{
  wl_argument *args = new wl_argument[v.size()];
  for(unsigned int c = 0; c < v.size(); c++)
    args[c] = v[c].get_c_argument();
  wl_resource_post_event_array(c_ptr(), opcode, args);
  delete[] args;
}

void resource_t::queue_event_array(uint32_t opcode, std::vector<argument_t> v)
{
  wl_argument *args = new wl_argument[v.size()];
  for(unsigned int c = 0; c < v.size(); c++)
    args[c] = v[c].get_c_argument();
  wl_resource_queue_event_array(c_ptr(), opcode, args);
  delete[] args;
}

void resource_t::post_error(uint32_t code, std::string msg)
{
  wl_resource_post_error(c_ptr(), code, "%s", msg.c_str());
}

void resource_t::post_no_memory()
{
  wl_resource_post_no_memory(c_ptr());
}

uint32_t resource_t::get_id()
{
  return wl_resource_get_id(c_ptr());
}

client_t resource_t::get_client()
{
  return client_t(wl_resource_get_client(c_ptr()));
}

unsigned int resource_t::get_version() const
{
  return wl_resource_get_version(resource);
}

std::string resource_t::get_class()
{
  return wl_resource_get_class(resource);
}

std::function<void()> &resource_t::on_destroy()
{
  return data->destroy;
}

//-----------------------------------------------------------------------------

event_loop_t::data_t *event_loop_t::wl_event_loop_get_user_data(wl_event_loop *client)
{
  wl_listener *listener = wl_event_loop_get_destroy_listener(client, destroy_func);
  if(listener)
    return reinterpret_cast<data_t*>(reinterpret_cast<listener_t*>(listener)->user);
  else
    return NULL;
}

void event_loop_t::destroy_func(wl_listener *listener, void *)
{
  event_loop_t::data_t *data = reinterpret_cast<event_loop_t::data_t*>(reinterpret_cast<listener_t*>(listener)->user);
  if(data->destroy)
    data->destroy();
}

int event_loop_t::event_loop_fd_func(int fd, uint32_t mask, void *data)
{
  std::function<int(int, uint32_t)> *f = reinterpret_cast<std::function<int(int, uint32_t)>*>(data);
  return (*f)(fd, mask);
}

int event_loop_t::event_loop_timer_func(void *data)
{
  std::function<int()> *f = reinterpret_cast<std::function<int()>*>(data);
  return (*f)();
}

int event_loop_t::event_loop_signal_func(int signal_number, void *data)
{
  std::function<int(int)> *f = reinterpret_cast<std::function<int(int)>*>(data);
  return (*f)(signal_number);
}

void event_loop_t::event_loop_idle_func(void *data)
{
  std::function<void()> *f = reinterpret_cast<std::function<void()>*>(data);
  (*f)();
}

void event_loop_t::init()
{
  data = new data_t;
  data->counter = 1;
  data->destroy_listener.user = data;
  data->destroy_listener.listener.notify = destroy_func;
  wl_event_loop_add_destroy_listener(event_loop, reinterpret_cast<wl_listener*>(&data->destroy_listener));
}

void event_loop_t::fini()
{
  data->counter--;
  if(data->counter == 0)
    {
      delete data;
      wl_event_loop_destroy(event_loop);
    }
}

event_loop_t::event_loop_t()
{
  event_loop = wl_event_loop_create();
  init();
}

event_loop_t::event_loop_t(wl_event_loop *p)
  : event_loop(p)
{
  data = wl_event_loop_get_user_data(c_ptr());
  if(!data)
    init();
  else
    data->counter++;
}

event_loop_t::~event_loop_t()
{
  fini();
}

event_loop_t::event_loop_t(const event_loop_t &p)
{
  event_loop = p.event_loop;
  data = p.data;
  data->counter++;
}

event_loop_t &event_loop_t::operator=(const event_loop_t& p)
{
  fini();
  event_loop = p.event_loop;
  data = p.data;
  data->counter++;
  return *this;
}

bool event_loop_t::operator==(const event_loop_t &c) const
{
  return c_ptr() == c.c_ptr();
}

wl_event_loop *event_loop_t::c_ptr() const
{
  if(!event_loop)
    throw std::runtime_error("event_loop is null.");
  return event_loop;
}

wayland::detail::any &event_loop_t::user_data()
{
  return data->user_data;
}

event_source_t event_loop_t::add_fd(int fd, uint32_t mask, const std::function<int(int, uint32_t)> &func)
{
  data->fd_funcs.push_back(func);
  return wl_event_loop_add_fd(event_loop, fd, mask, event_loop_t::event_loop_fd_func, &data->fd_funcs.back());
}

event_source_t event_loop_t::add_timer(const std::function<int()> &func)
{
  data->timer_funcs.push_back(func);
  return wl_event_loop_add_timer(event_loop, event_loop_t::event_loop_timer_func, &data->timer_funcs.back());
}

event_source_t event_loop_t::add_signal(int signal_number, const std::function<int(int)> &func)
{
  data->signal_funcs.push_back(func);
  return wl_event_loop_add_signal(event_loop, signal_number, event_loop_t::event_loop_signal_func, &data->signal_funcs.back());
}

event_source_t event_loop_t::add_idle(const std::function<void()> &func)
{
  data->idle_funcs.push_back(func);
  return wl_event_loop_add_idle(event_loop, event_loop_t::event_loop_idle_func, &data->idle_funcs.back());
}

const std::function<void()> &event_loop_t::on_destroy()
{
  return data->destroy;
}

int event_loop_t::dispatch(int timeout)
{
  return wl_event_loop_dispatch(c_ptr(), timeout);
}

void event_loop_t::dispatch_idle()
{
  wl_event_loop_dispatch_idle(c_ptr());
}

int event_loop_t::get_fd()
{
  return wl_event_loop_get_fd(c_ptr());
}

//-----------------------------------------------------------------------------

event_source_t::event_source_t(wl_event_source *p)
  : event_source(p)
{
}

int event_source_t::timer_update(int ms_delay)
{
  return wl_event_source_timer_update(event_source, ms_delay);
}

int event_source_t::fd_update(uint32_t mask)
{
  return wl_event_source_fd_update(event_source, mask);
}

void event_source_t::check()
{
  wl_event_source_check(event_source);
}
