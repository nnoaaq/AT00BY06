export const getUnixTime = () => {
  // PALAUTTAA NYKYISEN PÄIVÄMÄÄRÄN UNIX SEKUNTIT
  // FROM = VUOROKAUDEN ALKU
  // TO = VUOROKAUDEN LOPPU
  const timeNow = new Date();
  const startOfDay = new Date(timeNow);
  startOfDay.setHours(0, 0, 1, 0);
  const endOfDay = new Date(timeNow);
  endOfDay.setHours(23, 59, 59, 0);
  return {
    from: Math.floor(startOfDay.getTime()) / 1000,
    to: Math.floor(endOfDay.getTime() / 1000),
  };
};
export const convertTime = (unix: number) => {
  const dateObj = new Date(unix * 1000);
  return dateObj.toLocaleDateString("fi-FI", {
    day: "2-digit",
    month: "2-digit",
    hour: "2-digit",
    minute: "2-digit",
  });
};
