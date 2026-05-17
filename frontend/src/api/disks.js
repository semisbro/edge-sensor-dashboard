export const DISK_QUERY_KEY = ["disks"];
export const DISK_POLL_INTERVAL_MS = 10_000;

export async function fetchDiskInventory() {
  const response = await fetch("/api/disks");
  if (!response.ok) {
    throw new Error(`Backend returned ${response.status}`);
  }
  return response.json();
}

export function disksQueryOptions() {
  return {
    queryKey: DISK_QUERY_KEY,
    queryFn: fetchDiskInventory,
    refetchInterval: DISK_POLL_INTERVAL_MS,
    refetchIntervalInBackground: true,
  };
}
