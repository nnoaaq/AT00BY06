"use client";

import { useEffect, useState } from "react";

export const DatePicker = ({
  updateSelectedDays,
  reset,
}: {
  updateSelectedDays: (days: { from: number; to: number } | undefined) => void;
  reset: string | number;
}) => {
  // USE_STATET
  const [selectedStartDay, setSelectedStartDay] = useState<Date | null>(
    new Date(),
  );
  const [selectedEndDay, setSelectedEndDay] = useState<Date | null>(null);
  const [currentDate, setCurrentDate] = useState(new Date());
  const [showCalendar, setShowCalendar] = useState(false);
  // USE_EFFECT
  useEffect(() => {
    formatToUnix();
  }, [selectedStartDay, selectedEndDay]);
  useEffect(() => {
    setSelectedStartDay(null);
    setSelectedEndDay(null);
  }, [reset]);
  // FUNKTIOT
  const formatToUnix = () => {
    // LUODAAN JSON
    /*
      {
      from = UNIX-AIKALEIMA VUOROKAUDEN ALUSTA
  to = UNIX-AIKALEIMA VUOROKAUDEN LOPUSTA
      }
    */
    if (!selectedStartDay && !selectedEndDay) {
      updateSelectedDays(undefined);
      return;
    }
    let output: { from: number; to: number } | undefined = { from: 0, to: 0 };
    if (selectedStartDay && !selectedEndDay) {
      const startDate = new Date(selectedStartDay);
      const endDate = new Date(selectedStartDay);
      // VAIN YKSI PÄIVÄ VALITTU
      startDate?.setHours(0, 0, 0);
      output.from = Math.floor(startDate?.getTime() / 1000) || 0;
      endDate?.setHours(23, 59, 59);
      output.to = Math.floor(endDate?.getTime() / 1000) || 0;
    } else if (selectedStartDay && selectedEndDay) {
      const startDate = new Date(selectedStartDay);
      const endDate = new Date(selectedEndDay);
      output.from = Math.floor(startDate?.getTime() / 1000) || 0;
      endDate?.setHours(23, 59, 59);
      output.to = Math.floor(endDate?.getTime() / 1000) || 0;
    }
    updateSelectedDays(output);
  };
  const showMonth = (direction: number) => {
    setCurrentDate(
      new Date(
        currentDate.getFullYear(),
        currentDate.getMonth() - 1 * direction,
        1,
      ),
    );
  };
  const selectDays = (cellDate: Date) => {
    if (!selectedStartDay || (selectedStartDay && selectedEndDay)) {
      // EI ALKUPÄIVÄÄ TAI MOLEMMAT VALITTU
      setSelectedStartDay(cellDate);
      setSelectedEndDay(null);
    } else if (selectedStartDay && !selectedEndDay) {
      // VAIN ALKUPÄIVÄ VALITTU
      if (cellDate.getTime() === selectedStartDay.getTime()) {
        // KLIKATTU UUDELLEEN SAMAA ALKUPÄIVÄÄ
        setSelectedEndDay(cellDate);
      } else if (cellDate < selectedStartDay) {
        // KLIKATTU PÄIVÄ VANHEMPI KUIN VALITTU ALOITUSPÄIVÄ
        setSelectedStartDay(cellDate);
      } else {
        // KLIKATTU PÄIVÄ VANHEMPI KUIN VALITTU ALOITUSPÄIVÄ
        setSelectedEndDay(cellDate);
      }
    } else {
      // MOLEMMAT VALITTU > MUUTETAAN LOPPUPÄIVÄÄ
      setSelectedEndDay(cellDate);
    }
  };

  // NYKYINEN PÄIVÄ
  const currentDay = new Date();

  // DATE OLIOT
  const currentMonthDate = new Date(
    currentDate.getFullYear(),
    currentDate.getMonth(),
    1,
  );
  const previousMonthDate = new Date(
    currentDate.getFullYear(),
    currentDate.getMonth() - 1,
    1,
  );
  const nextMonthDate = new Date(
    currentDate.getFullYear(),
    currentDate.getMonth() + 1,
    1,
  );
  // NYKYINEN KUUKAUSI
  const currentMonth = {
    dayCount: new Date(
      currentMonthDate.getFullYear(),
      currentMonthDate.getMonth() + 1,
      0,
    ).getDate(),
    firstDayIndex:
      (new Date(
        currentMonthDate.getFullYear(),
        currentMonthDate.getMonth(),
        1,
      ).getDay() +
        6) %
      7,
    index: currentMonthDate.getMonth(),
    year: currentMonthDate.getFullYear(),
  };
  // EDELLINEN KUUKAUSI
  const previousMonth = {
    dayCount: new Date(
      previousMonthDate.getFullYear(),
      previousMonthDate.getMonth() + 1,
      0,
    ).getDate(),
    index: previousMonthDate.getMonth(),
    year: previousMonthDate.getFullYear(),
  };
  // SEURAAVA KUUKAUSI
  const nextMonth = {
    index: nextMonthDate.getMonth(),
    year: nextMonthDate.getFullYear(),
  };
  let cells = [];
  for (let i = 0; i < currentMonth.firstDayIndex; i++) {
    // EDELLISEN KUUKAUDEN PÄIVÄT
    // LASKETAAN EDELLISEN KUUKAUDEN PITUDUESTA TAAKSEPÄIN
    cells.push({
      day: previousMonth.dayCount - currentMonth.firstDayIndex + i + 1,
      month: previousMonth.index,
      year: previousMonth.year,
      isCurrentMonth: false,
      date: new Date(
        previousMonth.year,
        previousMonth.index,
        previousMonth.dayCount - currentMonth.firstDayIndex + i + 1,
      ),
    });
  }
  for (let i = 1; i <= currentMonth.dayCount; i++) {
    // NYKYISEN KUUKAUDEN PÄIVÄT
    cells.push({
      day: i,
      month: currentMonth.index,
      year: currentMonth.year,
      isCurrentMonth: true,
      date: new Date(currentMonth.year, currentMonth.index, i),
    });
  }
  const remainingDays = (7 - (cells.length % 7)) % 7;
  for (let i = 0; i < remainingDays; i++) {
    // SEURAAVAN KUUKAUDEN PÄIVÄT
    // i+1 TUOTTAA PÄIVÄMÄÄRÄN
    cells.push({
      day: i + 1,
      month: nextMonth.index,
      year: nextMonth.year,
      isCurrentMonth: false,
      date: new Date(nextMonth.year, nextMonth.index, i + 1),
    });
  }
  return (
    <div className="flex flex-col gap-2 relative w-full">
      <p className="text-foreground">Valitse päivämäärä</p>
      <button
        onClick={() => setShowCalendar(true)}
        className="w-full  text-start text-foreground cursor-pointer border border-border p-2 rounded-xl outline-none hover:ring-1 hover:ring-highlight focus:ring-1 focus:ring-highlight transition-all duration-100 ease-in-out flex gap-2 items-center justify-between"
      >
        <div className="flex gap-2 items-center">
          <svg
            xmlns="http://www.w3.org/2000/svg"
            fill="none"
            viewBox="0 0 24 24"
            strokeWidth={1.5}
            stroke="currentColor"
            className="size-5"
          >
            <path
              strokeLinecap="round"
              strokeLinejoin="round"
              d="M6.75 3v2.25M17.25 3v2.25M3 18.75V7.5a2.25 2.25 0 0 1 2.25-2.25h13.5A2.25 2.25 0 0 1 21 7.5v11.25m-18 0A2.25 2.25 0 0 0 5.25 21h13.5A2.25 2.25 0 0 0 21 18.75m-18 0v-7.5A2.25 2.25 0 0 1 5.25 9h13.5A2.25 2.25 0 0 1 21 11.25v7.5"
            />
          </svg>
          <p>
            {!selectedStartDay && !selectedEndDay ? (
              "Klikkaa avataksesi kalenterin"
            ) : (
              <>
                {selectedStartDay &&
                  `${selectedStartDay.toLocaleDateString("fi-FI", {
                    day: "2-digit",
                    month: "2-digit",
                  })} - `}
                {selectedEndDay &&
                  selectedEndDay.toLocaleDateString("fi-FI", {
                    day: "2-digit",
                    month: "2-digit",
                  })}
              </>
            )}
          </p>
        </div>
        <svg
          xmlns="http://www.w3.org/2000/svg"
          fill="none"
          viewBox="0 0 24 24"
          strokeWidth={1.5}
          stroke="currentColor"
          className="size-4"
        >
          <path
            strokeLinecap="round"
            strokeLinejoin="round"
            d="m19.5 8.25-7.5 7.5-7.5-7.5"
          />
        </svg>
      </button>
      <dialog
        onClose={() => setShowCalendar(false)}
        open={showCalendar}
        closedby={"any"}
        className="absolute top-20 p-2 bg-background rounded-md shadow-md border border-border w-80"
      >
        <div className="flex justify-between">
          <div onClick={() => showMonth(1)}>
            <svg
              xmlns="http://www.w3.org/2000/svg"
              fill="none"
              viewBox="0 0 24 24"
              strokeWidth={1.5}
              stroke="currentColor"
              className="size-5 text-lime-700 cursor-pointer hover:text-lime-900"
            >
              <path
                strokeLinecap="round"
                strokeLinejoin="round"
                d="M15.75 19.5 8.25 12l7.5-7.5"
              />
            </svg>
          </div>
          <h2 className="text-center text-zinc-300">
            {`${currentDate.toLocaleDateString("fi-Fi", { month: "long" })} ${currentDate.getFullYear()}`}
          </h2>
          <div onClick={() => showMonth(-1)}>
            <svg
              xmlns="http://www.w3.org/2000/svg"
              fill="none"
              viewBox="0 0 24 24"
              strokeWidth={1.5}
              stroke="currentColor"
              className="size-5 text-lime-700 cursor-pointer hover:text-lime-900"
            >
              <path
                strokeLinecap="round"
                strokeLinejoin="round"
                d="m8.25 4.5 7.5 7.5-7.5 7.5"
              />
            </svg>
          </div>
        </div>
        <div className="grid grid-cols-7  text-center text-foreground">
          <p>Ma</p>
          <p>Ti</p>
          <p>Ke</p>
          <p>To</p>
          <p>Pe</p>
          <p>La</p>
          <p>Su</p>
        </div>
        <div className="grid grid-cols-7  text-center ">
          {cells.map((cell, index) => {
            let styles = cell.isCurrentMonth
              ? "text-zinc-400"
              : "text-zinc-950";
            let rounded = "rounded-none";
            if (
              cell.date.getFullYear() === currentDay.getFullYear() &&
              cell.date.getMonth() === currentDay.getMonth() &&
              cell.date.getDate() === currentDay.getDate()
            ) {
              styles = "bg-blue-600";
              rounded = "rounded-xl";
            }
            if (cell.date.getTime() === selectedStartDay?.getTime())
              styles = "bg-zinc-950";
            if (cell.date.getTime() === selectedEndDay?.getTime())
              styles = "bg-zinc-950";
            if (
              selectedStartDay &&
              selectedEndDay &&
              cell.date > selectedStartDay &&
              cell.date < selectedEndDay
            ) {
              styles = "bg-zinc-700/90";
              rounded = "rounded-none";
            }
            if (
              cell.date.getTime() === selectedStartDay?.getTime() &&
              selectedStartDay &&
              !selectedEndDay
            ) {
              rounded = " rounded-xl";
              styles = "text-zinc-200 bg-zinc-950";
            }
            if (
              cell.date.getTime() === selectedEndDay?.getTime() &&
              selectedStartDay &&
              selectedEndDay
            ) {
              rounded = " rounded-r-xl";
              styles = "text-zinc-200 bg-zinc-950";
            }
            if (
              cell.date.getTime() === selectedStartDay?.getTime() &&
              selectedStartDay &&
              selectedEndDay
            ) {
              rounded = " rounded-l-xl";
              styles = "text-zinc-200 bg-zinc-950";
            }
            if (selectedStartDay?.getTime() === selectedEndDay?.getTime()) {
              rounded = "rounded-xl";
            }
            return (
              <div
                onClick={() => selectDays(cell.date)}
                className={`
                  cursor-pointer
                  hover:bg-amber-500
                  hover:text-zinc-950
                ${styles}
                ${rounded}
              `}
                key={index}
              >{`${cell.day}`}</div>
            );
          })}
        </div>
      </dialog>
    </div>
  );
};
