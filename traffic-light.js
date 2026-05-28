// Traffic Light Plugin - using event hook for message detection
const URL = "http://127.0.0.1:9876/";

let _lastCmd = "";
let _lastTime = 0;
const light = (cmd) => {
  const now = Date.now();
  if (cmd === _lastCmd && now - _lastTime < 500) return;
  _lastCmd = cmd;
  _lastTime = now;
  fetch(URL + cmd).catch(() => {});
};

let _started = false;

export default {
  id: "traffic-light",
  server: async () => {
    let working = false;
    let hadError = false;

    if (!_started) { _started = true; light("idle"); }

    return {
      // ---- Catch message events via bus (named hooks don't fire for message.*) ----
      "event": async ({ event }) => {
        const type = event?.type;
        if (type === "message.part.updated" && !working) {
          working = true; hadError = false;
          light("working");
        }
        if (type === "session.idle" && working && !hadError) {
          working = false;
          light("idle");
        }
        if (type === "session.error") {
          working = false; hadError = true;
          light("error");
        }
      },

      // ---- Tools executing -> yellow (backup if message event missed) ----
      "tool.execute.before": async (i) => {
        if (i?.tool === "question") {
          working = false;
          light("error");
        } else if (!working) {
          working = true;
          light("working");
        }
      },

      // ---- Done -> green ----
      "session.idle": async () => {
        if (hadError) return;
        working = false;
        light("idle");
      },

      // ---- Error / permission -> red ----
      "session.error": async () => {
        working = false; hadError = true;
        light("error");
      },
      "permission.asked": async () => {
        working = false;
        light("error");
      },
    };
  },
};
