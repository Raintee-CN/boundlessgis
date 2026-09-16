FROM postgis/postgis:18-3.6 AS builder

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
      build-essential \
      cmake \
      ninja-build \
      postgresql-server-dev-18 \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY maplibre-tile-spec /src/maplibre-tile-spec
COPY ext/boundlessgis /src/ext/boundlessgis

WORKDIR /src/ext/boundlessgis
RUN cmake -G Ninja -B build -S . \
      -DMLT_WITH_JSON=OFF \
      -DMLT_WITH_TESTS=OFF \
      -DMLT_WITH_TOOLS=OFF \
      -DCMAKE_BUILD_TYPE=Release \
    && cmake --build build --target boundlessgis \
    && cmake --install build

FROM postgis/postgis:18-3.6

COPY --from=builder /usr/lib/postgresql/18/lib/boundlessgis.so /usr/lib/postgresql/18/lib/boundlessgis.so
COPY --from=builder /usr/share/postgresql/18/extension/boundlessgis.control /usr/share/postgresql/18/extension/boundlessgis.control
COPY --from=builder /usr/share/postgresql/18/extension/boundlessgis--1.0.sql /usr/share/postgresql/18/extension/boundlessgis--1.0.sql
COPY docker/init/01-enable-extensions.sql /docker-entrypoint-initdb.d/01-enable-extensions.sql
COPY docker/init/02-smoke.sql /docker-entrypoint-initdb.d/02-smoke.sql
COPY docker/init/03-catalog.sql /docker-entrypoint-initdb.d/03-catalog.sql
COPY docker/test-st-asmlt.sql /opt/boundlessgis/test-st-asmlt.sql
