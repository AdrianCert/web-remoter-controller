#pragma once
#include "base.hpp"
#include "switch.hpp"
#include <string>

class MeshSwitchFunction : public BaseFunction {
  uint8_t _local_sw_id{0};
  LocalSwitch *_local_sw{nullptr};
  bool *_states_sw{nullptr};
  uint32_t *_node_ids{nullptr};
  uint8_t _size;

  bool (*send)(uint32_t, String){nullptr};

  bool sync_dev_state(uint8_t swid) {
    if (this->_size < swid)
      return false;

    if (swid == _local_sw_id) {
      _local_sw->update(_states_sw[swid]);
      return true;
    }

    String req = _states_sw[swid] ? "update::on" : "update::off";
    Serial.print("Sending the message \"");
    Serial.print(req.c_str());
    Serial.print("\" to node ");
    Serial.println(this->_node_ids[swid]);

    return this->emit(this->_node_ids[swid], req);
  }

public:
  String name() { return String("MeshSwitch"); }

  MeshSwitchFunction(uint8_t maxsw = 5) {
    this->_size = maxsw;
    this->_states_sw = (bool *)malloc(maxsw * sizeof(bool));
    this->_node_ids = (uint32_t *)malloc(maxsw * sizeof(uint32_t));
  }

  ~MeshSwitchFunction() {
    if (this->_states_sw != nullptr) {
      free(this->_states_sw);
      this->_states_sw = nullptr;
    }

    if (this->_node_ids != nullptr) {
      free(this->_node_ids);
      this->_node_ids = nullptr;
    }

    if (this->_local_sw != nullptr) {
      free(this->_local_sw);
      this->_local_sw = nullptr;
    }

    this->_size = 0;
  }

  void setup_send_proc(bool (*send)(uint32_t, String)) { this->send = send; }

  void setup_local_switch(uint8_t node_id, LocalSwitch *sw_function) {
    this->_local_sw_id = node_id;
    this->_local_sw = sw_function;
  }

  void free_local_switch() {
    this->_local_sw_id = 0;
    if (this->_local_sw != nullptr) {
      free(this->_local_sw);
      this->_local_sw = nullptr;
    }
  }

  void setup_remote_switch(uint8_t node_id) {
    for (uint8_t swid = 0; swid < _size; swid++) {
      if (_node_ids[swid] != 0)
        continue;

      _node_ids[swid] = node_id;
      _states_sw[swid] = false;

      this->emit(this->_node_ids[swid], "query::state");
      return;
    }
  }

  bool query_state_all() {
    for (uint8_t swid = 0; swid < _size; swid++) {
      if (_node_ids[swid] == 0)
        continue;

      this->emit(this->_node_ids[swid], "query::state");
    }
    return true;
  }

  void free_remote_switch(uint8_t node_id) {
    for (uint8_t swid = 0; swid < _size; swid++) {
      if (_node_ids[swid] != node_id)
        continue;

      _node_ids[swid] = 0;
      _states_sw[swid] = false;
      return;
    }
  }

  bool emit(uint32_t dest, String mes) {
    String req = name();
    req += "::";
    req += mes;
    return this->send(dest, req);
  }

  bool on(uint8_t swid) {
    if (this->_size < swid)
      return false;

    this->_states_sw[swid] = true;

    return sync_dev_state(swid);
  }

  bool off(uint8_t swid) {
    if (this->_size < swid)
      return false;

    this->_states_sw[swid] = true;

    return sync_dev_state(swid);
  }

  bool update(uint8_t swid, bool state) {
    if (this->_size < swid)
      return false;

    this->_states_sw[swid] = state;

    return sync_dev_state(swid);
  }

  bool run(String cmd, uint32_t req_id = 0) {
    uint8_t index{0};
    if (!cmd.startsWith(this->name(), index))
      return false;
    index = cmd.indexOf("::", index) + 2;

    if (cmd.startsWith("query::state", index)) {
      if (!req_id)
        return true;

      emit(req_id, _states_sw[_local_sw_id] ? "result::state::on"
                                            : "result::state::off");
    }

    if (cmd.startsWith("query::registration", index)) {
      if (!req_id)
        return true;

      this->setup_remote_switch(req_id);

      return true;
    }

    if (cmd.startsWith("result::state", index)) {
      index += 15;

      for (uint8_t swid = 0; swid < _size; swid++) {
        if (req_id != _node_ids[swid])
          continue;

        if (cmd.startsWith("on", index)) {
          _states_sw[swid] = true;
        }

        if (cmd.startsWith("off", index)) {
          _states_sw[swid] = false;
        }

        return true;
      }

      return true;
    }

    if (cmd.startsWith("update::on", index)) {
      on(_local_sw_id);
      return true;
    }

    if (cmd.startsWith("update::off", index)) {
      off(_local_sw_id);
      return true;
    }

    return false;
  }

public:

  struct SwitchStatesIter {
  private:
    uint8_t curr;
    bool _done{true};
    MeshSwitchFunction *obj;

  public:
    SwitchStatesIter(MeshSwitchFunction *obj) {
      _done = true;
      for (uint8_t ind = 0; ind < obj->_size; ind++) {
        if (obj->_node_ids[ind]) {
          _done = false;
          break;
        }
      }
    }

    bool done() {
      return _done;
    }

    bool next() {
      while (this->curr + 1 < obj->_size) {
        this->curr++;
        if (obj->_node_ids[curr]) {
          return true;
        }
      }
      _done = true;
      return false;
    }

    uint8_t id() { return curr; }

    bool state() { return obj->_states_sw[curr]; }

    bool node_id() { return obj->_node_ids[curr]; }

    bool local() { return obj->_local_sw_id == this->curr; }
  };

  SwitchStatesIter iter_states() {
    return SwitchStatesIter(this);
  }
};