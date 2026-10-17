// Small localStorage helpers that survive private mode.
export function load(key: string): string | null {
  try {
    return localStorage.getItem("petbox." + key);
  } catch {
    return null;
  }
}
export function save(key: string, value: string | null) {
  try {
    if (value === null) localStorage.removeItem("petbox." + key);
    else localStorage.setItem("petbox." + key, value);
  } catch {
    /* ignore */
  }
}
