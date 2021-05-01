FROM alpine:latest

RUN apk --no-cache add cmake make gcc g++ libc-dev linux-headers git bash zlib-dev curl-dev

ADD . /service
WORKDIR /service
#Start from clean repo
RUN rm -Rf /service/build || true
# Clean secret data & caches (Warning : deploying secret keys could be dangerous !)
RUN rm -f /service/data/\.*cache.json
ARG keepsecrets
RUN if [[ -z "$keepsecrets" ]]; then rm -f /service/data/.depositaddresses.json ;\
    rm -f /service/data/secret.json ;\
    else echo "Keeping secrets: warning do not deploy this docker image on a public space.";\
    fi

WORKDIR /service/build

RUN BUILD_MODE=Release && cmake -DCMAKE_BUILD_TYPE=${BUILD_MODE} ..
RUN make -j 4

ARG notest
# Check if tests should be run: 
RUN if [[ -z "$notest" ]]; then ctest; else echo "No tests."; fi

ENTRYPOINT [ "/service/build/coincenter" ]
