import type { Ctx } from "../app";
import { badges } from "../badges";
import { Bar } from "../ui";

export function Badges(c: Ctx) {
  const list = badges(c.st, c.history, c.me);
  const got = list.filter((b) => b.have).length;
  return (
    <>
      <p class="muted">
        {got} of {list.length} earned. All of them are earned by <i>not</i> using your phone.
      </p>
      <div class="badges">
        {list.map((b) => (
          <div class={"badge " + (b.have ? "have" : "locked")} key={b.name}>
            <div class="badge-icon">{b.icon}</div>
            <b>{b.name}</b>
            <span>{b.desc}</span>
            {!b.have && b.progress && (
              <>
                <Bar value={b.progress[0]} max={b.progress[1]} tone="dim" />
                <span class="small">
                  {b.progress[0]} / {b.progress[1]}
                </span>
              </>
            )}
          </div>
        ))}
      </div>
    </>
  );
}
