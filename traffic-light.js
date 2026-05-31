// Traffic Light Plugin - using event hook for message detection
import { createServer } from "http";
const URL = "http://127.0.0.1:9876/";

let _lastCmd = "";
let _lastTime = 0;
let _currentState = "idle";
const light = (cmd) => {
  const now = Date.now();
  if (cmd === _lastCmd && now - _lastTime < 500) return;
  _lastCmd = cmd;
  _lastTime = now;
  _currentState = cmd;
  fetch(URL + cmd).catch(() => {});
};

let _started = false;

// State server: lets the EXE query current status on startup
const stateServer = createServer((req, res) => {
  if (req.url === "/state") {
    res.writeHead(200, { "Content-Type": "application/json" });
    res.end(JSON.stringify({ state: _currentState }));
  } else {
    res.writeHead(404);
    res.end();
  }
});
stateServer.listen(9877, "127.0.0.1");

export default {
  id: "traffic-light",
  server: async () => {
    let working = false;
    let hadError = false;
    let waitingQuestion = false;

    if (!_started) { _started = true; light("idle"); }

    return {
      // ---- Catch message events via bus (named hooks don't fire for message.*) ----
      "event": async ({ event }) => {
        const type = event?.type;
        if (type === "message.part.updated" && !working && !waitingQuestion) {
          working = true; hadError = false;
          light("working");
        }
        if (type === "session.idle" && working && !hadError && !waitingQuestion) {
          working = false;
          light("idle");
        }
        if (type === "question.replied") {
          waitingQuestion = false;
          working = false;
          light("idle");
        }
        if (type === "session.error") {
          working = false; hadError = true; waitingQuestion = false;
          light("error");
        }
      },

      // ---- Tools executing -> yellow (backup if message event missed) ----
      "tool.execute.before": async (i) => {
        if (i?.tool === "question") {
          working = false; waitingQuestion = true;
          light("error");
        } else if (!working) {
          working = true;
          light("working");
        }
      },

      // ---- Done -> green (skip if waiting for question answer) ----
      "session.idle": async () => {
        if (hadError || waitingQuestion) return;
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
