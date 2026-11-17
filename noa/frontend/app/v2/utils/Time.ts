export const getCurrentDayUnixRange = () => {
  // FUNKTIO, JOKA PALAUTTAA NYKYISEN PÄIVÄN ALKU JA LOPPU AJAN UNIX-SEKUNTEINA
  const dateNow = new Date();

  // PÄIVÄN ALKU
  const startOfDay = new Date(
    dateNow.getFullYear(),
    dateNow.getMonth(),
    dateNow.getDate(),
    0,
    0,
    0,
  );
  // PÄIVÄN LOPPU
  const endOfDay = new Date(
    dateNow.getFullYear(),
    dateNow.getMonth(),
    dateNow.getDate(),
    23,
    59,
    59,
  );
  return {
    from: Math.floor(startOfDay.getTime() / 1000),
    to: Math.floor(endOfDay.getTime() / 1000),
  };
};
export const convertUnixTimeToDay = (unix: number) => {
  const dateObj = new Date(unix * 1000);
  return dateObj.toLocaleDateString("fi-FI", {
    day: "2-digit",
    month: "2-digit",
    hour: "2-digit",
    minute: "2-digit",
  });
};

export const convertDateToUnix = (date: string) => {
  // FUNKTIO, JOKA PALAUTTAA NYKYISEN PÄIVÄN ALKU JA LOPPU AJAN UNIX-SEKUNTEINA
  const [day, month, year] = date.split(".").map(Number);
  // PÄIVÄN ALKU
  const startOfDay = new Date(year, month - 1, day, 0, 0, 0);
  // PÄIVÄN LOPPU
  const endOfDay = new Date(year, month - 1, day, 23, 59, 59);
  return {
    from: Math.floor(startOfDay.getTime() / 1000),
    to: Math.floor(endOfDay.getTime() / 1000),
  };
};
