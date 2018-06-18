#ifndef simmer__resource_h
#define simmer__resource_h

#include <simmer/entity.h>
#include <simmer/process/arrival.h>

namespace simmer {

  /**
   *  Generic resource, a passive entity that comprises server + a queue.
   */
  class Resource : public Entity {
  public:
    /**
    * Constructor.
    * @param sim         a pointer to the simulator
    * @param name        the name
    * @param mon         int that indicates whether this entity must be monitored
    * @param capacity    server capacity (-1 means infinity)
    * @param queue_size  room in the queue (-1 means infinity)
    */
    Resource(Simulator* sim, const std::string& name, int mon, int capacity,
             int queue_size, bool queue_size_strict)
      : Entity(sim, name, mon), capacity(capacity), queue_size(queue_size),
        server_count(0), queue_count(0), queue_size_strict(queue_size_strict) {}

    /**
    * Reset the resource: server, queue
    */
    void reset() {
      server_count = 0;
      queue_count = 0;
    }

    /**
    * Seize resources.
    * @param   arrival  a pointer to the arrival trying to seize resources
    * @param   amount   the amount of resources needed
    *
    * @return  SUCCESS, ENQUEUE, REJECT
    */
    int seize(Arrival* arrival, int amount) {
      int status;
      // serve now
      if (first_in_line(arrival->order.get_priority()) &&
          room_in_server(amount, arrival->order.get_priority()))
      {
        insert_in_server(arrival, amount);
        status = SUCCESS;
      }
      // enqueue
      else if (room_in_queue(amount, arrival->order.get_priority())) {
        insert_in_queue(arrival, amount);
        status = ENQUEUE;
      }
      // reject
      else {
        if (sim->verbose) print(arrival->name, "REJECT");
        return REJECT;
      }

      arrival->register_entity(this);
      if (is_monitored())
        sim->mon->record_resource(name, sim->now(), server_count, queue_count, capacity, queue_size);
      return status;
    }

    /**
    * Release resources.
    * @param   arrival a pointer to the arrival that releases resources
    * @param   amount  the amount of resources released
    *
    * @return  SUCCESS
    */
    int release(Arrival* arrival, int amount) {
      remove_from_server(arrival, amount);
      arrival->unregister_entity(this);

      // serve another
      Task* task = new Task(sim, "Post-Release",
                            BIND(&Resource::post_release, this),
                            PRIORITY_RELEASE_POST);
      task->activate();

      return SUCCESS;
    }

    bool erase(Arrival* arrival, bool stay = false) {
      if (stay) {
        int amount = remove_from_server(arrival, -1);
        server_count += amount;
        return false;
      }

      if (!remove_from_queue(arrival)) {
        release(arrival, -1);
        return false;
      }

      if (is_monitored())
        sim->mon->record_resource(name, sim->now(), server_count, queue_count, capacity, queue_size);
      return true;
    }

    void set_capacity(int value) {
      if (capacity == value)
        return;
      int last = capacity;
      capacity = value;
      if (last >= 0  && (capacity > last || capacity < 0)) {
        // serve another
        while (queue_count)
          if (!try_serve_from_queue())
            break;
      } else if (last < 0 || capacity < last) {
        while (server_count > capacity)
          if (!try_free_server())
            break;
      }
      if (is_monitored())
        sim->mon->record_resource(name, sim->now(), server_count, queue_count, capacity, queue_size);
    }

    void set_queue_size(int value) {
      if (queue_size == value)
        return;
      int last = queue_size;
      queue_size = value;
      if (queue_size_strict && (last < 0 || (queue_size < last && queue_size >= 0))) {
        while (queue_count > queue_size)
          try_free_queue();
      }
      if (is_monitored())
        sim->mon->record_resource(name, sim->now(), server_count, queue_count, capacity, queue_size);
    }

    int get_capacity() const { return capacity; }
    int get_queue_size() const { return queue_size; }
    int get_server_count() const { return server_count; }
    int get_queue_count() const { return queue_count; }

  protected:
    int capacity;
    int queue_size;
    int server_count;       /**< number of arrivals being served */
    int queue_count;        /**< number of arrivals waiting */
    bool queue_size_strict;

    int post_release() {
      // serve another
      while (queue_count)
        if (!try_serve_from_queue())
          break;

      if (is_monitored())
        sim->mon->record_resource(name, sim->now(), server_count, queue_count, capacity, queue_size);
      return SUCCESS;
    }

    void print(const std::string& arrival, const std::string& status) const {
      sim->print("resource", name, "arrival", arrival, status);
    }

    virtual bool first_in_line(int priority) const = 0;
    virtual bool room_in_server(int amount, int priority) const = 0;
    virtual bool room_in_queue(int amount, int priority) const = 0;
    virtual int try_free_server() = 0;
    virtual int try_free_queue() = 0;
    virtual int try_serve_from_queue() = 0;
    virtual void insert_in_server(Arrival* arrival, int amount) = 0;
    virtual void insert_in_queue(Arrival* arrival, int amount) = 0;
    virtual int remove_from_server(Arrival* arrival, int amount) = 0;
    virtual int remove_from_queue(Arrival* arrival) = 0;
  };

} // namespace simmer

#endif