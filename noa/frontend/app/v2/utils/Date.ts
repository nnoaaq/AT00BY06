// PALAUTTAA PÄIVÄMÄÄRÄT, JOTKA ON KAHDEN UNIX AIKALEIMAN VÄLISSÄ
export const getDatesBetweenUnixTimestamps = ({
  from,
  to,
}: {
  from: number;
  to: number;
}) => {
  const dates: string[] = [];
  const fromDate = new Date(from * 1000);
  const toDate = new Date(to * 1000);
  fromDate.setHours(0, 0, 0, 0);
  toDate.setHours(0, 0, 0, 0);

  while (fromDate <= toDate) {
    const formatted = fromDate.toLocaleDateString("fi-FI");
    dates.push(formatted);
    fromDate.setDate(fromDate.getDate() + 1);
  }
  return dates;
};
