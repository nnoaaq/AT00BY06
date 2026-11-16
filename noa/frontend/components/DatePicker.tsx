"use client";

import { useEffect, useState } from "react";

export const DatePicker = ({
  updateSelectedDays,
  reset,
}: {
  updateSelectedDays: (days: { from: number; to: number } | undefined) => void;
  reset: string | number;
}) => {
  // NYKYINEN PÄIVÄ
  const currentDayDateObj = new Date();
  const currentDay = {
    year: currentDayDateObj.getFullYear(),
    month: currentDayDateObj.getMonth(),
    day: currentDayDateObj.getDate(),
  };

  // NYKYINEN KUUKAUSI
  const [currentMonthDateObj, setCurrentMonthDateObj] = useState(
    new Date(currentDay.year, currentDay.month, 1),
  );

  const currentMonth = {
    year: currentMonthDateObj.getFullYear(),
    month: currentMonthDateObj.getMonth(),
    day: currentDayDateObj.getDate(),
    monthName: currentMonthDateObj.toLocaleDateString("fi-FI", {
      month: "long",
    }),
    dayCount: new Date(
      currentMonthDateObj.getFullYear(),
      currentMonthDateObj.getMonth() + 1,
      0,
    ).getDate(),
    firstWeekDay:
      currentMonthDateObj.getDay() === 0 ? 6 : currentMonthDateObj.getDay() - 1,
    dateObj: currentMonthDateObj,
  };

  // EDELLINEN KUUKAUSI
  const previousMonthDateObj = new Date(
    currentMonth.year,
    currentMonth.month - 1,
    1,
  );
  const previousMonth = {
    dayCount: new Date(currentMonth.year, currentMonth.month, 0).getDate(),
    index: previousMonthDateObj.getMonth(),
    year: previousMonthDateObj.getFullYear(),
    month: previousMonthDateObj.getMonth(),
    dateObj: previousMonthDateObj,
  };

  // TULEVA KUUKAUSI
  const nextMonthDateObj = new Date(
    currentMonth.year,
    currentMonth.month + 1,
    1,
  );
  const nextMonth = {
    index: nextMonthDateObj.getMonth(),
    year: nextMonthDateObj.getFullYear(),
    month: nextMonthDateObj.getMonth(),
    dateObj: nextMonthDateObj,
  };

  let calendarCells = [];
  // EDELLISEN KUUKAUDEN PÄIVÄT
  for (let i = 0; i < currentMonth.firstWeekDay; i++) {
    calendarCells.push({
      year: previousMonth.year,
      month: previousMonth.month,
      day: previousMonth.dayCount - currentMonth.firstWeekDay + i + 1,
    });
  }
  // NYKYISEN KUUKAUDEN KAIKKI PÄIVÄT
  for (let i = 0; i < currentMonth.dayCount; i++) {
    calendarCells.push({
      year: currentMonth.year,
      month: currentMonth.month,
      day: i + 1,
    });
  }
  // TULEVAN KUUKAUDEN PÄIVÄT
  const remainingDays = (7 - (calendarCells.length % 7)) % 7;
  for (let i = 0; i < remainingDays; i++) {
    calendarCells.push({
      year: nextMonth.year,
      month: nextMonth.month,
      day: i + 1,
    });
  }

  // FUNKTIOT
  const changeMonth = (direction: number) => {
    setCurrentMonthDateObj(
      new Date(
        currentMonthDateObj.getFullYear(),
        currentMonthDateObj.getMonth() + direction,
        1,
      ),
    );
  };

  const handleClick = (year: number, month: number, day: number) => {
    const formatted = { year: year, month: month, day: day };
    const clickedValue = year * 10000 + month * 100 + day;

    const startValue = selectedStartDay
      ? selectedStartDay.year * 10000 +
        selectedStartDay.month * 100 +
        selectedStartDay.day
      : null;

    if (!selectedStartDay || (selectedStartDay && selectedEndDay)) {
      setSelectedStartDay(formatted);
      setSelectedEndDay(null);
    } else if (selectedStartDay && !selectedEndDay) {
      if (startValue && clickedValue < startValue) {
        setSelectedStartDay(formatted);
      } else {
        setSelectedEndDay(formatted);
      }
    }
  };

  // USE_STATET
  const [selectedStartDay, setSelectedStartDay] = useState<{
    year: number;
    month: number;
    day: number;
  } | null>(null);
  const [selectedEndDay, setSelectedEndDay] = useState<{
    year: number;
    month: number;
    day: number;
  } | null>(null);
  const [showCalendar, setShowCalendar] = useState(false);

  const formatToUnix = () => {
    if (!selectedStartDay && !selectedEndDay) {
      updateSelectedDays(undefined);
      return;
    }
    let output: { from: number; to: number } | undefined = { from: 0, to: 0 };
    if (selectedStartDay && !selectedEndDay) {
      const startDate = new Date(
        selectedStartDay.year,
        selectedStartDay.month,
        selectedStartDay.day,
      );
      const endDate = new Date(
        selectedStartDay.year,
        selectedStartDay.month,
        selectedStartDay.day,
      );
      startDate?.setHours(0, 0, 0);
      output.from = Math.floor(startDate?.getTime() / 1000) || 0;
      endDate?.setHours(23, 59, 59);
      output.to = Math.floor(endDate?.getTime() / 1000) || 0;
    } else if (selectedStartDay && selectedEndDay) {
      const startDate = new Date(
        selectedStartDay.year,
        selectedStartDay.month,
        selectedStartDay.day,
      );
      const endDate = new Date(
        selectedEndDay.year,
        selectedEndDay.month,
        selectedEndDay.day,
      );
      startDate?.setHours(0, 0, 0);
      output.from = Math.floor(startDate?.getTime() / 1000) || 0;
      endDate?.setHours(23, 59, 59);
      output.to = Math.floor(endDate?.getTime() / 1000) || 0;
    }
    updateSelectedDays(output);
  };

  // USE_EFFECTIT
  useEffect(() => {
    setSelectedStartDay(null);
    setSelectedEndDay(null);
  }, [reset]);

  useEffect(() => {
    formatToUnix();
  }, [selectedStartDay, selectedEndDay]);

  return (
    <div className="flex flex-col gap-2 relative w-full">
      <p className="text-foreground">Valitse päivämäärä</p>

      <div
        role="button"
        tabIndex={0}
        onClick={() => setShowCalendar(true)}
        onKeyDown={(e) => {
          if (e.key === "Enter" || e.key === " ") setShowCalendar(true);
        }}
        className="w-full text-start text-foreground cursor-pointer border border-border p-2 rounded-xl outline-none hover:ring-1 hover:ring-highlight focus:ring-1 focus:ring-highlight transition-all duration-100 ease-in-out flex gap-2 items-center justify-between"
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
                  new Date(
                    selectedStartDay.year,
                    selectedStartDay.month,
                    selectedStartDay.day,
                  ).toLocaleDateString("fi-FI", {
                    day: "2-digit",
                    month: "2-digit",
                  })}
                {selectedEndDay &&
                  ` - ${new Date(
                    selectedEndDay.year,
                    selectedEndDay.month,
                    selectedEndDay.day,
                  ).toLocaleDateString("fi-FI", {
                    day: "2-digit",
                    month: "2-digit",
                  })}`}
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

        <dialog
          closedby={"any"}
          onClose={() => setShowCalendar(false)}
          open={showCalendar}
          className="absolute top-20 w-full border border-border-dark rounded-xl shadow-lg p-2 bg-background text-foreground text-center"
        >
          <div className="flex justify-between">
            <button
              type="button"
              onClick={(e) => {
                e.stopPropagation();
                changeMonth(-1);
              }}
              className="cursor-pointer hover:text-highlight"
            >
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
                  d="m18.75 4.5-7.5 7.5 7.5 7.5m-6-15L5.25 12l7.5 7.5"
                />
              </svg>
            </button>

            <h3>{`${currentMonth.monthName} ${currentMonth.year}`}</h3>
            <button
              type="button"
              onClick={(e) => {
                e.stopPropagation();
                changeMonth(1);
              }}
              className="cursor-pointer hover:text-highlight"
            >
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
                  d="m5.25 4.5 7.5 7.5-7.5 7.5m6-15 7.5 7.5-7.5 7.5"
                />
              </svg>
            </button>
          </div>
          <div className="grid grid-cols-7 text-foreground">
            <p>ma</p>
            <p>ti</p>
            <p>ke</p>
            <p>to</p>
            <p>pe</p>
            <p>la</p>
            <p>su</p>
          </div>
          <div className="grid grid-cols-7">
            {calendarCells.map((calendarCell, index) => {
              return (
                <div
                  key={index}
                  onClick={(e) => {
                    e.stopPropagation();
                    handleClick(
                      calendarCell.year,
                      calendarCell.month,
                      calendarCell.day,
                    );
                  }}
                  className={`
                    ${
                      calendarCell.day === selectedStartDay?.day &&
                      calendarCell.month === selectedStartDay?.month &&
                      calendarCell.year === selectedStartDay?.year
                        ? selectedEndDay
                          ? "bg-highlight text-background rounded-l-lg"
                          : "bg-highlight text-background rounded-lg"
                        : calendarCell.day === selectedEndDay?.day &&
                            calendarCell.month === selectedEndDay?.month &&
                            calendarCell.year === selectedEndDay?.year
                          ? "bg-highlight text-background rounded-r-lg"
                          : calendarCell.day === currentDay.day &&
                              calendarCell.month === currentDay.month &&
                              calendarCell.year === currentDay.year
                            ? "bg-calendar-current text-background rounded-xl"
                            : selectedStartDay &&
                                selectedEndDay &&
                                calendarCell.year * 10000 +
                                  calendarCell.month * 100 +
                                  calendarCell.day >
                                  selectedStartDay.year * 10000 +
                                    selectedStartDay.month * 100 +
                                    selectedStartDay.day &&
                                calendarCell.year * 10000 +
                                  calendarCell.month * 100 +
                                  calendarCell.day <
                                  selectedEndDay.year * 10000 +
                                    selectedEndDay.month * 100 +
                                    selectedEndDay.day
                              ? "bg-calendar-secondary"
                              : calendarCell.month != currentMonth.month
                                ? "text-calendar-secondary"
                                : ""
                    }
                    p-1
                    hover:bg-highlight hover:rounded-lg cursor-pointer hover:text-background transition-colors duration-300
                    h-9 flex items-center justify-center text-sm
                  `}
                >
                  {calendarCell.day}
                </div>
              );
            })}
          </div>
        </dialog>
      </div>
    </div>
  );
};
