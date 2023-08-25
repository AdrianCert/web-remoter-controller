#pragma once
#include "base.hpp"
#include "switch.hpp"
#include <string>

#define DEFAULT_VALUE 112

struct SwitchState {
  bool local;
  uint32_t node_id;
  uint8_t store_id;
  bool state;
};

typedef void (*cb_iter_sw)(String &data, SwitchState &swd);
typedef void (*cb_change_sw)(SwitchState &swd);

class MeshSwitchFunction : public BaseFunction {
  uint8_t _local_sw_id{0};
  LocalSwitch *_local_sw{nullptr};
  bool _states_sw[10];
  uint32_t _node_ids[10];
  uint8_t _size;
  cb_change_sw _on_change_sw;

  bool (*send)(uint32_t, String){nullptr};

  bool sync_dev_state(uint8_t swid) {
    if (this->_size < swid)
      return false;

    tigger_on_change(swid);

    if (swid == _local_sw_id) {
      _local_sw->update(_states_sw[swid]);
      return true;
    }

    String req = _states_sw[swid] ? "update::on" : "update::off";
    Serial.printf("SENDING to %d the message \"%s\"\n", this->_node_ids[swid],
                  req.c_str());

    if (!this->_node_ids[swid]) {
      return false;
    }

    return this->emit(this->_node_ids[swid], req);
  }

  SwitchState pick_state(uint8_t swid) {
    SwitchState res{.local = this->_local_sw_id == swid,
                    .node_id = this->_node_ids[swid],
                    .store_id = swid,
                    .state = this->_states_sw[swid]};
    return res;
  }

  void tigger_on_change(uint8_t swid) {
    if (_on_change_sw) {
      SwitchState swd = pick_state(swid);
      _on_change_sw(swd);
    }
  }

public:
  String name() { return String("MeshSwitch"); }

  MeshSwitchFunction(uint8_t maxsw = 5) {
    this->_size = maxsw;
    for (uint8_t ind = 0; ind < _size; ind++) {
      _states_sw[ind] = false;
      _node_ids[ind] = 0;
    }
  }

  ~MeshSwitchFunction() {
    if (this->_local_sw != nullptr) {
      free(this->_local_sw);
      this->_local_sw = nullptr;
    }

    this->_size = 0;
  }

  void on_change(cb_change_sw cb) { _on_change_sw = cb; }

  void setup_send_proc(bool (*send)(uint32_t, String)) { this->send = send; }

  void setup_local_switch(uint32_t node_id, LocalSwitch *sw_function) {
    this->_local_sw = sw_function;
    setup_remote_switch(node_id, 1);
  }

  void free_local_switch() {
    this->_local_sw_id = 0;
    if (this->_local_sw != nullptr) {
      free(this->_local_sw);
      this->_local_sw = nullptr;
    }
  }

  void setup_remote_switch(uint32_t node_id, uint8_t local = 0) {
    for (uint8_t swid = 0; swid < _size; swid++) {
      if (_node_ids[swid] != 0)
        continue;

      _node_ids[swid] = node_id;
      _states_sw[swid] = false;
      if (local) {
        this->_local_sw_id = swid;
      }

      this->emit(this->_node_ids[swid], "query::state");
      return;
    }
  }

  bool query_state_all() {
    this->emit(0, "query::state");
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

    Serial.printf("MeshSwitchFunction::on(%u)\n", swid);
    return sync_dev_state(swid);
  }

  bool off(uint8_t swid) {
    if (this->_size < swid)
      return false;

    this->_states_sw[swid] = false;

    Serial.printf("MeshSwitchFunction::off(%u)\n", swid);
    return sync_dev_state(swid);
  }

  bool update(uint8_t swid, bool state) {
    if (this->_size < swid)
      return false;

    this->_states_sw[swid] = state;

    Serial.printf("MeshSwitchFunction::update(%u, %u)\n", swid, state);
    return sync_dev_state(swid);
  }

  void iter(String &data, cb_iter_sw func, uint8_t iters = 10,
            uint8_t skips = 0) {
    uint8_t count = 0;
    for (uint8_t id = 0; id < _size; id++) {
      if (_node_ids[id]) {
        count++;
        if (count <= skips) {
          continue;
        }
        if (count > iters) {
          return;
        }
        Serial.printf("MeshSwitchFunction::iter::procesing at $%d\n", id);
        SwitchState swd = pick_state(id);
        func(data, swd);
      }
    }
  }

  bool run(String cmd, uint32_t req_id = 0) {
    Serial.printf("RUN MeshSwitchFunction::run < %s, %d>\n", cmd.c_str(),
                  req_id);

    uint8_t index{0};
    if (!cmd.startsWith(this->name(), index)) {
      return false;
    }
    index = cmd.indexOf("::", index) + 2;

    if (cmd.startsWith("query::state", index)) {
      if (!req_id) {
        return true;
      }

      emit(req_id, _states_sw[_local_sw_id] ? "result::state::on"
                                            : "result::state::off");
      return true;
    }

    if (cmd.startsWith("query::registration", index)) {
      if (!req_id) {
        return true;
      }

      this->setup_remote_switch(req_id);

      return true;
    }

    if (cmd.startsWith("result::state", index)) {
      index += 15;

      int8_t swid = index_sni(req_id);

      if (swid == -1) {
        setup_remote_switch(req_id);
      }

      swid = index_sni(req_id);

      if (cmd.startsWith("on", index)) {
        _states_sw[swid] = true;
        tigger_on_change(swid);
      }

      if (cmd.startsWith("off", index)) {
        _states_sw[swid] = false;
        tigger_on_change(swid);
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

  int8_t index_sni(uint32_t node_id) {
    for (uint8_t swid = 0; swid < _size; swid++) {
      if (node_id == _node_ids[swid]) {
        return swid;
      }
    }
    return -1;
  }

  uint8_t index_lsf() { return _local_sw_id; }

  bool state(uint8_t swid) { return this->_states_sw[swid]; }
  uint32_t node_id(uint8_t swid) { return this->_node_ids[swid]; }
  bool local(uint8_t swid) { return this->_local_sw_id == swid; }
  uint8_t size() { return this->_size; }
};
