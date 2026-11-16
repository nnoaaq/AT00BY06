import { Loader } from "../components/Loader";
import { Suspense } from "react";
export default async function Home() {
  return (
    <div className="p-2">
      <Suspense fallback={<p>Sensorit latautuvat edelleen...</p>}>
        <Loader />
      </Suspense>
    </div>
  );
}
