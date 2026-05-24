import { useState, useEffect } from "react";

const ROOMS = [
  { id: "401", wing: "A" }, { id: "402", wing: "A" }, { id: "403", wing: "A" },
  { id: "404", wing: "A" }, { id: "405", wing: "A" }, { id: "406", wing: "A" },
  { id: "407", wing: "A" }, { id: "408", wing: "A" }, { id: "409", wing: "A" },
  { id: "410", wing: "A" },
  { id: "411", wing: "B" }, { id: "412", wing: "B" }, { id: "413", wing: "B" },
  { id: "414", wing: "B" }, { id: "415", wing: "B" }, { id: "416", wing: "B" },
  { id: "417", wing: "B" }, { id: "418", wing: "B" }, { id: "419", wing: "B" },
  { id: "420", wing: "B" },
  { id: "421", wing: "C" }, { id: "422", wing: "C" }, { id: "423", wing: "C" },
  { id: "424", wing: "C" }, { id: "425", wing: "C" }, { id: "426", wing: "C" },
  { id: "427", wing: "C" }, { id: "428", wing: "C" }, { id: "429", wing: "C" },
  { id: "430", wing: "C" },
  { id: "431", wing: "D" }, { id: "432", wing: "D" }, { id: "433", wing: "D" },
  { id: "434", wing: "D" }, { id: "435", wing: "D" }, { id: "436", wing: "D" },
  { id: "437", wing: "D" }, { id: "438", wing: "D" }, { id: "439", wing: "D" },
  { id: "440", wing: "D" },
];

const STATUS = {
  NORMAL: "normal",
  OCCLUSION: "occlusion",
  LEAK: "leak",
};

const WING_COLORS = {
  A: "#1e3a5f",
  B: "#1e4f3a",
  C: "#3a1e5f",
  D: "#5f3a1e",
};

function IVDrip({ status }) {
  const color =
    status === STATUS.NORMAL ? "#4ade80"
    : status === STATUS.OCCLUSION ? "#f59e0b"
    : "#ef4444";

  const animate = status === STATUS.NORMAL;

  return (
    <svg width="28" height="44" viewBox="0 0 28 44" fill="none">
      {/* Bag body */}
      <rect x="4" y="2" width="20" height="22" rx="5" fill={color} opacity="0.25" stroke={color} strokeWidth="1.5"/>
      {/* Liquid level */}
      <rect
        x="5.5"
        y={status === STATUS.OCCLUSION ? "14" : "10"}
        width="17"
        height={status === STATUS.LEAK ? "6" : "13"}
        rx="3"
        fill={color}
        opacity="0.55"
        style={{
          transition: "all 0.5s ease",
        }}
      />
      {/* Tube */}
      <line x1="14" y1="24" x2="14" y2="36" stroke={color} strokeWidth="2"/>
      {/* Drip chamber */}
      <ellipse cx="14" cy="37" rx="4" ry="3.5" fill={color} opacity="0.2" stroke={color} strokeWidth="1.2"/>
      {/* Animated drop */}
      {animate && (
        <circle cx="14" cy="37" r="1.5" fill={color}>
          <animate attributeName="cy" values="32;42" dur="1.4s" repeatCount="indefinite"/>
          <animate attributeName="opacity" values="1;0" dur="1.4s" repeatCount="indefinite"/>
        </circle>
      )}
      {/* Blocked indicator for occlusion */}
      {status === STATUS.OCCLUSION && (
        <rect x="11" y="33" width="6" height="3" rx="1.5" fill={color}/>
      )}
      {/* Leak droplets */}
      {status === STATUS.LEAK && (
        <>
          <circle cx="7" cy="19" r="2" fill={color} opacity="0.7">
            <animate attributeName="cy" values="19;28" dur="0.9s" repeatCount="indefinite"/>
            <animate attributeName="opacity" values="0.7;0" dur="0.9s" repeatCount="indefinite"/>
          </circle>
          <circle cx="21" cy="21" r="1.5" fill={color} opacity="0.7">
            <animate attributeName="cy" values="21;30" dur="1.1s" repeatCount="indefinite" begin="0.3s"/>
            <animate attributeName="opacity" values="0.7;0" dur="1.1s" repeatCount="indefinite" begin="0.3s"/>
          </circle>
        </>
      )}
    </svg>
  );
}

function RoomCard({ room, status, lastChanged, onClick }) {
  const isAlert = status !== STATUS.NORMAL;
  const label = status === STATUS.NORMAL ? "Normal" : status === STATUS.OCCLUSION ? "Occlusion" : "Leak Detected";

  const borderColor =
    status === STATUS.NORMAL ? "rgba(74,222,128,0.25)"
    : status === STATUS.OCCLUSION ? "rgba(245,158,11,0.7)"
    : "rgba(239,68,68,0.7)";

  const glowColor =
    status === STATUS.NORMAL ? "none"
    : status === STATUS.OCCLUSION ? "0 0 18px 2px rgba(245,158,11,0.4)"
    : "0 0 18px 2px rgba(239,68,68,0.5)";

  const bgColor =
    status === STATUS.NORMAL ? "rgba(255,255,255,0.03)"
    : status === STATUS.OCCLUSION ? "rgba(245,158,11,0.07)"
    : "rgba(239,68,68,0.08)";

  const textColor =
    status === STATUS.NORMAL ? "#4ade80"
    : status === STATUS.OCCLUSION ? "#f59e0b"
    : "#ef4444";

  return (
    <div
      onClick={onClick}
      style={{
        background: bgColor,
        border: `1.5px solid ${borderColor}`,
        boxShadow: glowColor,
        borderRadius: "10px",
        padding: "14px 10px 10px",
        cursor: "pointer",
        transition: "all 0.3s ease",
        display: "flex",
        flexDirection: "column",
        alignItems: "center",
        gap: "6px",
        position: "relative",
        animation: isAlert ? "pulse-border 2s infinite" : "none",
        minWidth: "100px",
      }}
    >
      {isAlert && (
        <div style={{
          position: "absolute",
          top: "6px",
          right: "7px",
          width: "8px",
          height: "8px",
          borderRadius: "50%",
          background: textColor,
          animation: "blink 1s infinite",
        }}/>
      )}
      <div style={{
        fontFamily: "'DM Mono', monospace",
        fontSize: "11px",
        letterSpacing: "0.12em",
        color: "rgba(255,255,255,0.45)",
        textTransform: "uppercase",
      }}>
        Room
      </div>
      <div style={{
        fontFamily: "'DM Mono', monospace",
        fontSize: "22px",
        fontWeight: "700",
        color: "rgba(255,255,255,0.9)",
        letterSpacing: "0.04em",
        lineHeight: 1,
      }}>
        {room.id}
      </div>

      <IVDrip status={status} />

      <div style={{
        fontFamily: "'DM Mono', monospace",
        fontSize: "11px",
        fontWeight: "600",
        color: textColor,
        letterSpacing: "0.08em",
        textTransform: "uppercase",
        textAlign: "center",
        lineHeight: 1.3,
      }}>
        {label}
      </div>

      <div style={{
        fontSize: "9px",
        color: "rgba(255,255,255,0.2)",
        fontFamily: "'DM Mono', monospace",
        letterSpacing: "0.05em",
      }}>
        {lastChanged}
      </div>
    </div>
  );
}

function Modal({ room, status, onClose, onResolve }) {
  if (!room) return null;

  const isAlert = status !== STATUS.NORMAL;
  const textColor = status === STATUS.NORMAL ? "#4ade80" : status === STATUS.OCCLUSION ? "#f59e0b" : "#ef4444";
  const label = status === STATUS.NORMAL ? "Normal" : status === STATUS.OCCLUSION ? "IV Occlusion" : "IV Leak Detected";
  const [confirming, setConfirming] = useState(false);

  const handleResolveClick = () => {
    if (!confirming) { setConfirming(true); return; }
    onResolve();
  };

  return (
    <div
      onClick={onClose}
      style={{
        position: "fixed", inset: 0,
        background: "rgba(0,0,0,0.72)",
        display: "flex", alignItems: "center", justifyContent: "center",
        zIndex: 1000, backdropFilter: "blur(5px)",
      }}
    >
      <div
        onClick={e => e.stopPropagation()}
        style={{
          background: "#0c1520",
          border: `1.5px solid ${textColor}44`,
          borderRadius: "18px",
          padding: "32px",
          width: "360px",
          fontFamily: "'DM Mono', monospace",
          boxShadow: `0 0 50px ${textColor}18, 0 24px 48px rgba(0,0,0,0.6)`,
        }}
      >
        {/* Header */}
        <div style={{ display: "flex", justifyContent: "space-between", alignItems: "flex-start", marginBottom: "22px" }}>
          <div>
            <div style={{ color: "rgba(255,255,255,0.35)", fontSize: "10px", letterSpacing: "0.15em", textTransform: "uppercase", marginBottom: "4px" }}>
              Wing {room.wing} · Floor 4
            </div>
            <div style={{ color: "white", fontSize: "38px", fontWeight: "700", lineHeight: 1 }}>
              Room {room.id}
            </div>
          </div>
          <IVDrip status={status} />
        </div>

        {/* Status box */}
        <div style={{
          background: `${textColor}0e`,
          border: `1px solid ${textColor}40`,
          borderRadius: "10px",
          padding: "14px 16px",
          marginBottom: "22px",
        }}>
          <div style={{ display: "flex", alignItems: "center", gap: "8px", marginBottom: "8px" }}>
            <div style={{ width: "8px", height: "8px", borderRadius: "50%", background: textColor, animation: isAlert ? "blink 1s infinite" : "none" }}/>
            <div style={{ color: textColor, fontSize: "13px", fontWeight: "600", letterSpacing: "0.08em" }}>
              {label}
            </div>
          </div>
          {status === STATUS.OCCLUSION && (
            <>
              <div style={{ color: "rgba(255,255,255,0.5)", fontSize: "11px", lineHeight: 1.6 }}>
                Flow restriction detected in IV line. Recommended checks:
              </div>
              <ul style={{ color: "rgba(255,255,255,0.4)", fontSize: "11px", lineHeight: 1.8, marginTop: "6px", paddingLeft: "16px" }}>
                <li>Inspect tubing for kinks or bends</li>
                <li>Verify roller clamp is fully open</li>
                <li>Check for positional occlusion at catheter site</li>
              </ul>
            </>
          )}
          {status === STATUS.LEAK && (
            <>
              <div style={{ color: "rgba(255,255,255,0.5)", fontSize: "11px", lineHeight: 1.6 }}>
                Fluid loss detected at IV connection. Recommended checks:
              </div>
              <ul style={{ color: "rgba(255,255,255,0.4)", fontSize: "11px", lineHeight: 1.8, marginTop: "6px", paddingLeft: "16px" }}>
                <li>Inspect all luer-lock connections</li>
                <li>Check insertion site for infiltration</li>
                <li>Examine tubing for cracks or punctures</li>
              </ul>
            </>
          )}
          {status === STATUS.NORMAL && (
            <div style={{ color: "rgba(255,255,255,0.5)", fontSize: "11px", lineHeight: 1.6 }}>
              IV flow rate is within expected parameters. No intervention required.
            </div>
          )}
        </div>

        {/* Resolution section */}
        {isAlert && (
          <div style={{ marginBottom: "16px" }}>
            {!confirming ? (
              <button
                onClick={handleResolveClick}
                style={{
                  width: "100%",
                  padding: "13px",
                  borderRadius: "10px",
                  border: "1px solid rgba(74,222,128,0.4)",
                  background: "rgba(74,222,128,0.08)",
                  color: "#4ade80",
                  fontFamily: "'DM Mono', monospace",
                  fontSize: "12px",
                  fontWeight: "600",
                  letterSpacing: "0.1em",
                  textTransform: "uppercase",
                  cursor: "pointer",
                  transition: "all 0.2s",
                }}
                onMouseEnter={e => e.target.style.background = "rgba(74,222,128,0.16)"}
                onMouseLeave={e => e.target.style.background = "rgba(74,222,128,0.08)"}
              >
                ✓ Mark Issue as Resolved
              </button>
            ) : (
              <div style={{
                border: "1px solid rgba(74,222,128,0.3)",
                borderRadius: "10px",
                padding: "16px",
                background: "rgba(74,222,128,0.05)",
              }}>
                <div style={{ color: "rgba(255,255,255,0.7)", fontSize: "11px", letterSpacing: "0.06em", marginBottom: "12px", textAlign: "center" }}>
                  Confirm issue has been physically inspected and resolved?
                </div>
                <div style={{ display: "flex", gap: "8px" }}>
                  <button
                    onClick={handleResolveClick}
                    style={{
                      flex: 1, padding: "10px",
                      borderRadius: "8px",
                      border: "1px solid rgba(74,222,128,0.5)",
                      background: "rgba(74,222,128,0.15)",
                      color: "#4ade80",
                      fontFamily: "'DM Mono', monospace",
                      fontSize: "11px", fontWeight: "700",
                      letterSpacing: "0.08em", textTransform: "uppercase",
                      cursor: "pointer",
                    }}
                  >
                    Yes, Resolved
                  </button>
                  <button
                    onClick={() => setConfirming(false)}
                    style={{
                      flex: 1, padding: "10px",
                      borderRadius: "8px",
                      border: "1px solid rgba(255,255,255,0.1)",
                      background: "transparent",
                      color: "rgba(255,255,255,0.4)",
                      fontFamily: "'DM Mono', monospace",
                      fontSize: "11px",
                      letterSpacing: "0.08em", textTransform: "uppercase",
                      cursor: "pointer",
                    }}
                  >
                    Cancel
                  </button>
                </div>
              </div>
            )}
          </div>
        )}

        <button
          onClick={onClose}
          style={{
            width: "100%", padding: "10px",
            borderRadius: "8px",
            border: "1px solid rgba(255,255,255,0.08)",
            background: "rgba(255,255,255,0.03)",
            color: "rgba(255,255,255,0.35)",
            fontFamily: "'DM Mono', monospace",
            fontSize: "11px",
            letterSpacing: "0.1em", textTransform: "uppercase",
            cursor: "pointer",
          }}
        >
          Dismiss
        </button>
      </div>
    </div>
  );
}

export default function App() {
  const [statuses, setStatuses] = useState(() => {
    const init = {};
    ROOMS.forEach(r => { init[r.id] = STATUS.NORMAL; });
    // Randomly assign 7 rooms an issue
    const shuffled = [...ROOMS].sort(() => Math.random() - 0.5);
    const issueTypes = [STATUS.OCCLUSION, STATUS.LEAK];
    shuffled.slice(0, 7).forEach(r => {
      init[r.id] = issueTypes[Math.floor(Math.random() * 2)];
    });
    return init;
  });

  const [timestamps, setTimestamps] = useState(() => {
    const ts = {};
    ROOMS.forEach(r => { ts[r.id] = "just now"; });
    return ts;
  });

  const [selected, setSelected] = useState(null);
  const [time, setTime] = useState(new Date());

  useEffect(() => {
    const interval = setInterval(() => setTime(new Date()), 1000);
    return () => clearInterval(interval);
  }, []);

  const handleResolve = (roomId) => {
    setStatuses(prev => ({ ...prev, [roomId]: STATUS.NORMAL }));
    setTimestamps(prev => ({
      ...prev,
      [roomId]: new Date().toLocaleTimeString([], { hour: "2-digit", minute: "2-digit" }),
    }));
    setSelected(null);
  };

  const alerts = ROOMS.filter(r => statuses[r.id] !== STATUS.NORMAL);
  const occulsions = alerts.filter(r => statuses[r.id] === STATUS.OCCLUSION).length;
  const leaks = alerts.filter(r => statuses[r.id] === STATUS.LEAK).length;

  const selectedRoom = selected ? ROOMS.find(r => r.id === selected) : null;

  const wings = ["A", "B", "C", "D"];

  return (
    <div style={{
      minHeight: "100vh",
      background: "#080d14",
      color: "white",
      fontFamily: "'DM Mono', monospace",
    }}>
      <style>{`
        @import url('https://fonts.googleapis.com/css2?family=DM+Mono:wght@300;400;500;600;700&display=swap');
        @keyframes blink { 0%,100% { opacity:1; } 50% { opacity:0.1; } }
        @keyframes pulse-border { 0%,100% { opacity:1; } 50% { opacity:0.7; } }
        ::-webkit-scrollbar { width: 4px; } 
        ::-webkit-scrollbar-track { background: #080d14; }
        ::-webkit-scrollbar-thumb { background: #1e293b; border-radius: 2px; }
      `}</style>

      {/* Header */}
      <div style={{
        borderBottom: "1px solid rgba(255,255,255,0.07)",
        padding: "16px 32px",
        display: "flex",
        alignItems: "center",
        justifyContent: "space-between",
        background: "rgba(255,255,255,0.02)",
        position: "sticky",
        top: 0,
        zIndex: 100,
        backdropFilter: "blur(12px)",
      }}>
        <div style={{ display: "flex", alignItems: "center", gap: "20px" }}>
          <div>
            <div style={{ fontSize: "9px", color: "rgba(255,255,255,0.3)", letterSpacing: "0.2em", textTransform: "uppercase" }}>
              IIIV UI Demo
            </div>
            <div style={{ fontSize: "18px", fontWeight: "700", letterSpacing: "0.04em", color: "white", lineHeight: 1.1 }}>
              IV Monitor · Floor 4
            </div>
          </div>
        </div>

        <div style={{ display: "flex", gap: "20px", alignItems: "center" }}>
          {/* Stats */}
          {[
            { label: "Active Rooms", value: ROOMS.length, color: "rgba(255,255,255,0.7)" },
            { label: "Normal", value: ROOMS.length - alerts.length, color: "#4ade80" },
            { label: "Occlusions", value: occulsions, color: "#f59e0b" },
            { label: "Leaks", value: leaks, color: "#ef4444" },
          ].map(stat => (
            <div key={stat.label} style={{ textAlign: "center" }}>
              <div style={{ fontSize: "22px", fontWeight: "700", color: stat.color, lineHeight: 1 }}>
                {stat.value}
              </div>
              <div style={{ fontSize: "9px", color: "rgba(255,255,255,0.3)", letterSpacing: "0.1em", textTransform: "uppercase" }}>
                {stat.label}
              </div>
            </div>
          ))}
          <div style={{
            padding: "6px 12px",
            background: "rgba(255,255,255,0.04)",
            borderRadius: "6px",
            border: "1px solid rgba(255,255,255,0.08)",
            fontSize: "12px",
            color: "rgba(255,255,255,0.5)",
            letterSpacing: "0.06em",
          }}>
            {time.toLocaleTimeString([], { hour: "2-digit", minute: "2-digit", second: "2-digit" })}
          </div>
        </div>
      </div>

      {/* Alert Banner */}
      {alerts.length > 0 && (
        <div style={{
          background: "rgba(239,68,68,0.08)",
          borderBottom: "1px solid rgba(239,68,68,0.2)",
          padding: "10px 32px",
          display: "flex",
          gap: "16px",
          alignItems: "center",
          flexWrap: "wrap",
        }}>
          <div style={{ fontSize: "11px", color: "#ef4444", letterSpacing: "0.1em", fontWeight: "600" }}>
            ● ALERTS —
          </div>
          {alerts.map(r => (
            <button
              key={r.id}
              onClick={() => setSelected(r.id)}
              style={{
                background: statuses[r.id] === STATUS.OCCLUSION ? "rgba(245,158,11,0.12)" : "rgba(239,68,68,0.12)",
                border: `1px solid ${statuses[r.id] === STATUS.OCCLUSION ? "#f59e0b" : "#ef4444"}55`,
                color: statuses[r.id] === STATUS.OCCLUSION ? "#f59e0b" : "#ef4444",
                borderRadius: "4px",
                padding: "3px 10px",
                fontFamily: "'DM Mono', monospace",
                fontSize: "11px",
                cursor: "pointer",
                letterSpacing: "0.06em",
              }}
            >
              {r.id} · {statuses[r.id] === STATUS.OCCLUSION ? "Occlusion" : "Leak"}
            </button>
          ))}
        </div>
      )}

      {/* Main Grid */}
      <div style={{ padding: "28px 32px" }}>
        {wings.map(wing => {
          const wingRooms = ROOMS.filter(r => r.wing === wing);
          return (
            <div key={wing} style={{ marginBottom: "32px" }}>
              <div style={{
                display: "flex",
                alignItems: "center",
                gap: "12px",
                marginBottom: "14px",
              }}>
                <div style={{
                  width: "4px",
                  height: "20px",
                  background: `${WING_COLORS[wing]}`,
                  borderRadius: "2px",
                  filter: "brightness(2.5)",
                }}/>
                <div style={{
                  fontSize: "11px",
                  color: "rgba(255,255,255,0.35)",
                  letterSpacing: "0.2em",
                  textTransform: "uppercase",
                }}>
                  Wing {wing} · {wingRooms.length} Rooms
                </div>
                <div style={{ flex: 1, height: "1px", background: "rgba(255,255,255,0.05)" }}/>
              </div>

              <div style={{
                display: "flex",
                flexWrap: "wrap",
                gap: "12px",
              }}>
                {wingRooms.map(room => (
                  <RoomCard
                    key={room.id}
                    room={room}
                    status={statuses[room.id]}
                    lastChanged={timestamps[room.id]}
                    onClick={() => setSelected(room.id)}
                  />
                ))}
              </div>
            </div>
          );
        })}
      </div>

      {/* Legend */}
      <div style={{
        borderTop: "1px solid rgba(255,255,255,0.06)",
        padding: "16px 32px",
        display: "flex",
        gap: "24px",
        alignItems: "center",
      }}>
        <div style={{ fontSize: "10px", color: "rgba(255,255,255,0.25)", letterSpacing: "0.12em", textTransform: "uppercase" }}>
          Legend:
        </div>
        {[
          { color: "#4ade80", label: "Normal Flow" },
          { color: "#f59e0b", label: "Occlusion — flow blocked" },
          { color: "#ef4444", label: "Leak — fluid loss" },
        ].map(l => (
          <div key={l.label} style={{ display: "flex", alignItems: "center", gap: "7px" }}>
            <div style={{ width: "10px", height: "10px", borderRadius: "50%", background: l.color }}/>
            <div style={{ fontSize: "11px", color: "rgba(255,255,255,0.4)", letterSpacing: "0.06em" }}>
              {l.label}
            </div>
          </div>
        ))}
        <div style={{ flex: 1, textAlign: "right", fontSize: "10px", color: "rgba(255,255,255,0.2)", letterSpacing: "0.08em" }}>
          Click any room to inspect · Click alert rooms to resolve
        </div>
      </div>

      {/* Modal */}
      <Modal
        room={selectedRoom}
        status={selectedRoom ? statuses[selectedRoom.id] : null}
        onClose={() => setSelected(null)}
        onResolve={() => selectedRoom && handleResolve(selectedRoom.id)}
      />
    </div>
  );
}
