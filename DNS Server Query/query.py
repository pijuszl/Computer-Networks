import sys
import dns.resolver

def query_dnss(hostname, record_type='A'):
    try:
        # Check host file first
        with open('C:\Windows\System32\drivers\etc\hosts') as f:
            for line in f:
                if line.strip().startswith('#'):
                    continue
                fields = line.strip().split()
                if len(fields) < 2:
                    continue
                if fields[1] == hostname:
                    ip_addresses = [fields[0]]
                    return ip_addresses

        # If not found in host file, send DNS query
        resolver = dns.resolver.Resolver()
        answer = resolver.resolve(hostname, record_type)
        ip_addresses = [rdata.address for rdata in answer]

        return ip_addresses

    except dns.resolver.NXDOMAIN:
        print(f"{hostname} does not exist.")
    except dns.resolver.NoAnswer:
        print(f"No {record_type} record found for {hostname}.")
    except dns.exception.Timeout:
        print("DNS query timed out.")
    except Exception as e:
        print(f"An error occurred: {e}")

    return None

########################################################################

if len(sys.argv) == 2:
    domain = sys.argv[1]
else:
    domain = "google.com"
ip_addresses = query_dnss(domain)

print(f"Domain name's {domain} IP addresses:")
print(ip_addresses)